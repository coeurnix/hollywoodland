#include "HLSfx.h"
#include "HLGame.h"
#include "HLData.h"
#include "HLBridge.h"
#include "HLCharacter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ------------------------------------------------------------------------------------------------------------- backends
// Commands: Play (handle, file, volume, pan -1..1, rate, loop, loop frame count, fade-in), Set (volume/pan ramp), Stop
// (fade-out), Master, StopAll. The web batches them into one bridge call per frame; native applies them to the mixer.
namespace
{
	const TCHAR* SfxSettingsKey = TEXT("hollywoodland_sfx");
}

#if PLATFORM_WASM
namespace HLAudio
{
	FString Cmds;
	void Line(const FString& L) { if (!Cmds.IsEmpty()) Cmds += TEXT("\n"); Cmds += L; }
	int32 Play(int32 H, const FString& File, int32, float Vol, float Pan, float Rate, bool bLoop, int32 Samples, float Fade)
	{
		Line(FString::Printf(TEXT("p|%d|%s|%.3f|%.2f|%.3f|%d|%d|%.2f"), H, *File, Vol, Pan, Rate, bLoop ? 1 : 0, Samples, Fade));
		return H;
	}
	void Set(int32 H, float Vol, float Pan, float Ramp) { Line(FString::Printf(TEXT("s|%d|%.3f|%.2f|%.2f"), H, Vol, Pan, Ramp)); }
	void Stop(int32 H, float Fade) { Line(FString::Printf(TEXT("x|%d|%.2f"), H, Fade)); }
	void Master(float V) { Line(FString::Printf(TEXT("m|%.3f"), V)); }
	void StopAll() { Line(TEXT("a")); }
	void Preload(const FString& File) { Line(TEXT("l|") + File); }
	void Flush() { if (!Cmds.IsEmpty()) { HLBridge::Audio(Cmds); Cmds.Reset(); } }
	void Init() {}
	FString Stats() { return FString(); }
}
#elif PLATFORM_WINDOWS
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Async/Async.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmsystem.h>
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#pragma warning(push, 0)
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DRMP3_API static
#include "dr_mp3.h"
#pragma warning(pop)
THIRD_PARTY_INCLUDES_END

namespace HLAudio
{
	constexpr int32 Rate = 44100, BufFrames = 512, NumBufs = 6;
	constexpr int32 EncoderDelay = 1105; // libmp3lame 576 + decoder 529, when the decoder keeps it

	struct FClip { TArray<int16> Pcm; int32 Ch = 1, Frames = 0, LoopStart = 0, LoopLen = 0; };
	typedef TSharedPtr<FClip, ESPMode::ThreadSafe> FClipPtr;
	struct FVoice { int32 H = 0; FClipPtr Clip; double Pos = 0; float Rate = 1, Vol = 0, VolTarget = 0, VolStep = 0, Pan = 0, PanTarget = 0; bool bLoop = false, bStopAtZero = false; };
	struct FCmd { int32 Type = 0, H = 0; FClipPtr Clip; float Vol = 0, Pan = 0, Rate = 1, Fade = 0; bool bLoop = false; };

	FCriticalSection CmdLock, ClipLock;
	TArray<FCmd> Pending;
	TMap<FString, FClipPtr> Clips;
	TSet<FString> Decoding;

	FClipPtr Decode(const FString& Path, int32 LoopSamples)
	{
		TArray<uint8> Data;
		if (!FFileHelper::LoadFileToArray(Data, *Path)) return nullptr;
		drmp3_config Cfg;
		drmp3_uint64 N = 0;
		drmp3_int16* P = drmp3_open_memory_and_read_pcm_frames_s16(Data.GetData(), Data.Num(), &Cfg, &N, nullptr);
		if (!P || !N) { if (P) drmp3_free(P, nullptr); return nullptr; }
		FClipPtr C = MakeShared<FClip, ESPMode::ThreadSafe>();
		C->Ch = FMath::Clamp((int32)Cfg.channels, 1, 2);
		C->Frames = (int32)N;
		C->Pcm.SetNumUninitialized(C->Frames * C->Ch);
		FMemory::Memcpy(C->Pcm.GetData(), P, C->Pcm.Num() * sizeof(int16));
		drmp3_free(P, nullptr);
		C->LoopLen = C->Frames;
		if (LoopSamples > 0)
		{
			// the master's exact length is known: skip the encoder delay when the decoder kept it, loop exactly that many frames
			C->LoopStart = C->Frames - LoopSamples >= EncoderDelay ? EncoderDelay : 0;
			C->LoopLen = FMath::Min(LoopSamples, C->Frames - C->LoopStart - 1);
		}
		return C;
	}
	/** One-shots decode now (short); loops decode on a worker and return null until ready (callers retry). */
	FClipPtr Clip(const FString& Path, int32 LoopSamples, bool bAsync)
	{
		{
			FScopeLock L(&ClipLock);
			if (const FClipPtr* C = Clips.Find(Path)) return *C;
			if (bAsync)
			{
				if (Decoding.Contains(Path)) return nullptr;
				Decoding.Add(Path);
			}
		}
		if (!bAsync)
		{
			FClipPtr C = Decode(Path, LoopSamples);
			FScopeLock L(&ClipLock);
			Clips.Add(Path, C);
			return C;
		}
		Async(EAsyncExecution::ThreadPool, [Path, LoopSamples]()
		{
			FClipPtr C = Decode(Path, LoopSamples);
			FScopeLock L(&ClipLock);
			Clips.Add(Path, C);
			Decoding.Remove(Path);
		});
		return nullptr;
	}

	class FMixer : public FRunnable
	{
	public:
		std::atomic<bool> bQuit{ false };
		std::atomic<int32> Mixed{ 0 }, Peak{ 0 }, NumVoices{ 0 };
		float Master = 0.7f;
		TArray<FVoice> Voices;
		uint32 Run() override
		{
			HANDLE Ev = CreateEventW(nullptr, 0, 0, nullptr);
			WAVEFORMATEX F = {};
			F.wFormatTag = WAVE_FORMAT_PCM; F.nChannels = 2; F.nSamplesPerSec = Rate; F.wBitsPerSample = 16; F.nBlockAlign = 4; F.nAvgBytesPerSec = Rate * 4;
			HWAVEOUT Wo = nullptr;
			if (waveOutOpen(&Wo, WAVE_MAPPER, &F, (DWORD_PTR)Ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
			{
				UE_LOG(LogTemp, Warning, TEXT("HL sfx: no audio device (waveOutOpen failed)"));
				CloseHandle(Ev);
				return 0;
			}
			static int16 Buf[NumBufs][BufFrames * 2];
			WAVEHDR Hdr[NumBufs] = {};
			for (int32 i = 0; i < NumBufs; ++i)
			{
				FMemory::Memzero(Buf[i], sizeof(Buf[i]));
				Hdr[i].lpData = (LPSTR)Buf[i]; Hdr[i].dwBufferLength = sizeof(Buf[i]);
				waveOutPrepareHeader(Wo, &Hdr[i], sizeof(WAVEHDR));
				waveOutWrite(Wo, &Hdr[i], sizeof(WAVEHDR));
			}
			int32 Next = 0;
			while (!bQuit)
			{
				WaitForSingleObject(Ev, 30);
				while (!bQuit && (Hdr[Next].dwFlags & WHDR_DONE))
				{
					Mix(Buf[Next], BufFrames);
					waveOutWrite(Wo, &Hdr[Next], sizeof(WAVEHDR));
					Next = (Next + 1) % NumBufs;
				}
			}
			waveOutReset(Wo);
			for (int32 i = 0; i < NumBufs; ++i) waveOutUnprepareHeader(Wo, &Hdr[i], sizeof(WAVEHDR));
			waveOutClose(Wo);
			CloseHandle(Ev);
			return 0;
		}
		void Stop() override { bQuit = true; }

		void Apply()
		{
			TArray<FCmd> Cmds;
			{ FScopeLock L(&CmdLock); Cmds = MoveTemp(Pending); Pending.Reset(); }
			auto Find = [this](int32 H) { return Voices.FindByPredicate([H](const FVoice& V) { return V.H == H; }); };
			auto Step = [](float From, float To, float S) { return S > 0.001f ? FMath::Max(1e-6f, FMath::Abs(To - From) / (S * Rate)) : 10.f; };
			for (FCmd& C : Cmds)
			{
				if (C.Type == 0 && C.Clip.IsValid() && C.Clip->Frames > 1)
				{
					FVoice V; V.H = C.H; V.Clip = C.Clip; V.Rate = C.Rate; V.bLoop = C.bLoop; V.Pan = V.PanTarget = C.Pan;
					V.Pos = C.bLoop ? C.Clip->LoopStart : 0;
					V.VolTarget = C.Vol; V.Vol = C.Fade > 0 ? 0 : C.Vol; V.VolStep = Step(V.Vol, C.Vol, C.Fade);
					if (Voices.Num() < 48) Voices.Add(V);
				}
				else if (C.Type == 1) { if (FVoice* V = Find(C.H)) { V->VolTarget = C.Vol; V->VolStep = Step(V->Vol, C.Vol, C.Fade); V->PanTarget = C.Pan; } }
				else if (C.Type == 2) { if (FVoice* V = Find(C.H)) { V->VolTarget = 0; V->VolStep = Step(V->Vol, 0, C.Fade); V->bStopAtZero = true; } }
				else if (C.Type == 3) Master = C.Vol;
				else if (C.Type == 4) Voices.Reset();
			}
		}
		void Mix(int16* Out, int32 N)
		{
			Apply();
			float Acc[BufFrames * 2];
			FMemory::Memzero(Acc, sizeof(float) * N * 2);
			for (int32 vi = Voices.Num() - 1; vi >= 0; --vi)
			{
				FVoice& V = Voices[vi];
				const FClip& C = *V.Clip;
				const int16* S = C.Pcm.GetData();
				const int32 End = V.bLoop ? C.LoopStart + C.LoopLen : C.Frames - 1;
				bool bDone = false;
				const float P0 = V.Pan, P1 = V.PanTarget;
				for (int32 i = 0; i < N; ++i)
				{
					if (V.Vol != V.VolTarget) V.Vol = V.Vol < V.VolTarget ? FMath::Min(V.VolTarget, V.Vol + V.VolStep) : FMath::Max(V.VolTarget, V.Vol - V.VolStep);
					if (V.bStopAtZero && V.Vol <= 0) { bDone = true; break; }
					const float Pan = P0 + (P1 - P0) * (float(i) / N);
					const int32 I0 = (int32)V.Pos;
					int32 I1 = I0 + 1;
					if (I1 >= End) I1 = V.bLoop ? C.LoopStart : I0;
					const float Fr = float(V.Pos - I0);
					if (C.Ch == 1)
					{
						const float X = (S[I0] + (S[I1] - S[I0]) * Fr) * (V.Vol / 32768.f);
						const float A = (Pan + 1) * (PI / 4);
						Acc[i * 2] += X * FMath::Cos(A) * 1.2f; Acc[i * 2 + 1] += X * FMath::Sin(A) * 1.2f;
					}
					else
					{
						const float L = (S[I0 * 2] + (S[I1 * 2] - S[I0 * 2]) * Fr) * (V.Vol / 32768.f);
						const float R = (S[I0 * 2 + 1] + (S[I1 * 2 + 1] - S[I0 * 2 + 1]) * Fr) * (V.Vol / 32768.f);
						Acc[i * 2] += L * FMath::Min(1.f, 1 - Pan); Acc[i * 2 + 1] += R * FMath::Min(1.f, 1 + Pan);
					}
					V.Pos += V.Rate;
					if (V.Pos >= End) { if (V.bLoop) V.Pos -= C.LoopLen; else { bDone = true; break; } }
				}
				V.Pan = P1;
				if (bDone) Voices.RemoveAtSwap(vi);
			}
			for (int32 i = 0; i < N * 2; ++i)
			{
				float X = Acc[i] * Master;
				if (FMath::Abs(X) > 0.7f) X = FMath::Sign(X) * (0.7f + 0.3f * FMath::Tanh((FMath::Abs(X) - 0.7f) / 0.3f)); // soft knee
				Out[i] = (int16)FMath::Clamp(X * 32767.f, -32767.f, 32767.f);
				if (FMath::Abs((int32)Out[i]) > Peak) Peak = FMath::Abs((int32)Out[i]);
			}
			++Mixed; NumVoices = Voices.Num();
		}
	};
	FMixer* Mixer = nullptr;
	FRunnableThread* Thread = nullptr;

	void Init()
	{
		if (Mixer) return;
		Mixer = new FMixer();
		Thread = FRunnableThread::Create(Mixer, TEXT("HLSfxMixer"), 0, TPri_AboveNormal);
	}
	void Push(FCmd&& C) { FScopeLock L(&CmdLock); if (Pending.Num() < 512) Pending.Add(MoveTemp(C)); }
	int32 Play(int32 H, const FString& Path, int32, float Vol, float Pan, float Rt, bool bLoop, int32 Samples, float Fade)
	{
		FClipPtr C = Clip(Path, bLoop ? Samples : 0, bLoop);
		if (!C.IsValid()) return 0;
		FCmd Cmd; Cmd.Type = 0; Cmd.H = H; Cmd.Clip = C; Cmd.Vol = Vol; Cmd.Pan = Pan; Cmd.Rate = Rt; Cmd.bLoop = bLoop; Cmd.Fade = Fade;
		Push(MoveTemp(Cmd));
		return H;
	}
	void Set(int32 H, float Vol, float Pan, float Ramp) { FCmd C; C.Type = 1; C.H = H; C.Vol = Vol; C.Pan = Pan; C.Fade = Ramp; Push(MoveTemp(C)); }
	void Stop(int32 H, float Fade) { FCmd C; C.Type = 2; C.H = H; C.Fade = Fade; Push(MoveTemp(C)); }
	void Master(float V) { FCmd C; C.Type = 3; C.Vol = V; Push(MoveTemp(C)); }
	void StopAll() { FCmd C; C.Type = 4; Push(MoveTemp(C)); }
	void Preload(const FString&) {}
	void Flush() {}
	FString Stats()
	{
		if (!Mixer) return TEXT("no mixer");
		const FString S = FString::Printf(TEXT("mixed %d buffers, %d voices, peak %.1f dBFS"), Mixer->Mixed.load(), Mixer->NumVoices.load(),
			Mixer->Peak > 0 ? 20.f * FMath::LogX(10.f, Mixer->Peak / 32767.f) : -99.f);
		Mixer->Peak = 0;
		return S;
	}
}
#else
namespace HLAudio
{
	void Init() {}
	int32 Play(int32, const FString&, int32, float, float, float, bool, int32, float) { return 0; }
	void Set(int32, float, float, float) {}
	void Stop(int32, float) {}
	void Master(float) {}
	void StopAll() {}
	void Preload(const FString&) {}
	void Flush() {}
	FString Stats() { return FString(); }
}
#endif

// ---------------------------------------------------------------------------------------------------------------- FHLSfx

void FHLSfx::Init(bool bMuteByDefault)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/sfx.json"))) ||
		!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() || !Root->HasTypedField<EJson::Object>(TEXT("sounds")))
	{
		HLBridge::Log(TEXT("HL sfx: Content/Data/sfx.json missing or invalid; no sound effects"));
		return;
	}
	// voice lines (tools/dialoguegen): same format, files beside the sound effects; optional
	{
		FString VoText;
		TSharedPtr<FJsonObject> Vo;
		if (FFileHelper::LoadFileToString(VoText, *(FPaths::ProjectContentDir() / TEXT("Data/vo.json"))) &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VoText), Vo) && Vo.IsValid() && Vo->HasTypedField<EJson::Object>(TEXT("sounds")))
			for (const auto& KV : Vo->GetObjectField(TEXT("sounds"))->Values) Root->GetObjectField(TEXT("sounds"))->SetField(KV.Key, KV.Value);
	}
	for (const auto& KV : Root->GetObjectField(TEXT("sounds"))->Values)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (!KV.Value->TryGetObject(O)) continue;
		FHLSfxDef D;
		D.Id = KV.Key;
		(*O)->TryGetStringField(TEXT("cat"), D.Cat);
		double V = 0;
		if ((*O)->TryGetNumberField(TEXT("vol"), V)) D.Vol = (float)V;
		if ((*O)->TryGetNumberField(TEXT("range"), V)) D.Range = (float)V;
		if ((*O)->TryGetNumberField(TEXT("pitch"), V)) D.Pitch = (float)V;
		(*O)->TryGetBoolField(TEXT("loop"), D.bLoop);
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if ((*O)->TryGetArrayField(TEXT("files"), Arr)) for (const TSharedPtr<FJsonValue>& F : *Arr) D.Files.Add(F->AsString());
		if ((*O)->TryGetArrayField(TEXT("samples"), Arr)) for (const TSharedPtr<FJsonValue>& F : *Arr) D.Samples.Add((int32)F->AsNumber());
		if (D.Files.Num()) Defs.Add(D.Id, MoveTemp(D));
	}
#if PLATFORM_WASM
	Folder = HLBridge::AudioBase() + TEXT("sfx/");
#else
	for (const FString& Dir : { FPaths::ProjectDir() / TEXT("Sfx"), FPaths::ProjectDir() / TEXT("../sfx") })
	{
		FString Full = FPaths::ConvertRelativePathToFull(Dir) + TEXT("/");
		if (FPaths::DirectoryExists(Full) && FPaths::FileExists(Full + TEXT("ui_click_0.mp3"))) { FPaths::MakePlatformFilename(Full); Folder = Full; break; }
	}
	if (Folder.IsEmpty()) { HLBridge::Log(TEXT("HL sfx: no sfx folder found (expected <ProjectDir>/Sfx or <repo>/sfx)")); return; }
#endif
	HLAudio::Init();
	bReady = true;
	HLBridge::Log(FString::Printf(TEXT("HL sfx: %d sounds"), Defs.Num()));
	// the web fetches on first use; warm the sounds that must be instant
	for (const auto& KV : Defs)
		if (KV.Value.Cat == TEXT("ui") || KV.Value.Cat == TEXT("combat") || KV.Value.Cat == TEXT("player") || KV.Value.Cat == TEXT("school"))
			for (const FString& F : KV.Value.Files) HLAudio::Preload(Folder + F);
	if (bMuteByDefault) { Volume = 0; HLAudio::Master(0); HLAudio::Flush(); return; }
	HLAudio::Master(Volume);
	HLBridge::StorageGet(SfxSettingsKey, [this](const FHLBridgeResult& R)
	{
		TSharedPtr<FJsonObject> O;
		double V = 0;
		if (R.Ok() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(R.Body), O) && O.IsValid() && O->TryGetNumberField(TEXT("sfx"), V))
			SetVolume((float)V, false);
	});
	HLAudio::Flush();
}

void FHLSfx::SetVolume(float V, bool bSave)
{
	Volume = FMath::Clamp(V, 0.f, 1.f);
	HLAudio::Master(Volume);
	if (bSave) HLBridge::StorageSet(SfxSettingsKey, FString::Printf(TEXT("{\"sfx\":%.2f}"), Volume));
}

void FHLSfx::StopAll()
{
	HLAudio::StopAll();
	Loops.Reset();
	AmbId.Reset(); AmbHandle = 0;
}

int32 FHLSfx::Play(const FHLSfxDef& D, int32 Variant, float Vol, float Pan, float Rate, float FadeS)
{
	if (!bReady || Volume <= 0 || !D.Files.Num()) return 0;
	Variant = FMath::Clamp(Variant, 0, D.Files.Num() - 1);
	const int32 H = NextHandle++;
	return HLAudio::Play(H, Folder + D.Files[Variant], Variant, FMath::Clamp(Vol, 0.f, 1.5f), FMath::Clamp(Pan, -1.f, 1.f), Rate, D.bLoop,
		D.Samples.IsValidIndex(Variant) ? D.Samples[Variant] : 0, FadeS);
}

void FHLSfx::Ui(const TCHAR* Id, float Vol)
{
	const FHLSfxDef* D = Defs.Find(Id);
	if (!D || D->bLoop) return;
	int32& N = ThisFrame.FindOrAdd(Id);
	if (N >= 1) return;
	++N;
	Play(*D, FMath::RandRange(0, D->Files.Num() - 1), D->Vol * Vol, 0, 1 + FMath::FRandRange(-D->Pitch, D->Pitch), 0);
}

bool FHLSfx::Spatial(int32 Area, const FVector2D& P, float Z, float Range, float& OutGain, float& OutPan) const
{
	if (Area != LArea) return false;
	const FVector2D D2 = P - LPos;
	const float Dist = FMath::Sqrt(D2.SizeSquared() + FMath::Square(Z - LZ) * 0.5f);
	if (Dist >= Range) return false;
	const float T = FMath::Max(0.f, Dist - 2.f) / FMath::Max(1.f, Range - 2.f);
	OutGain = FMath::Square(1.f - T);
	const float Yr = FMath::DegreesToRadians(LYaw);
	const FVector2D Right(-FMath::Sin(Yr), FMath::Cos(Yr));
	OutPan = Dist > 0.5f ? FVector2D::DotProduct(D2 / FMath::Max(Dist, 0.01f), Right) * FMath::Clamp(Dist / 4.f, 0.f, 0.85f) : 0.f;
	return OutGain > 0.01f;
}

void FHLSfx::Event(FHLGame& G, const FHLSoundEvent& E)
{
	const FHLSfxDef* D = Defs.Find(E.Id);
	if (!D || D->bLoop) return;
	int32& N = ThisFrame.FindOrAdd(E.Id);
	if (N >= 2) return;
	float Gain = 1, Pan = 0;
	const bool bMine = E.UnitId && E.UnitId == G.PlayerUnitId;
	// placed events of 2D sounds (a bell at the ring) still need the listener nearby
	if (E.Area >= 0 && !bMine && !Spatial(E.Area, E.Pos, E.Z, D->Range > 0 ? D->Range : 70.f, Gain, Pan)) return;
	if (D->Range <= 0) { Gain = FMath::Max(Gain, 0.35f); Pan *= 0.5f; }
	if (E.UnitId)
	{
		// one bark of a kind per unit at a time
		const bool bVoice = E.Id.EndsWith(TEXT("_hurt")) || E.Id.EndsWith(TEXT("_attack")) || E.Id.Contains(TEXT("_hurt_"));
		const uint64 Key = (uint64(uint32(E.UnitId)) << 32) | GetTypeHash(E.Id);
		double& Last = LastByUnit.FindOrAdd(Key);
		if (Clock - Last < (bVoice ? 0.9 : 0.05)) return;
		Last = Clock;
	}
	++N;
	const int32 Var = E.Variant >= 0 ? E.Variant : FMath::RandRange(0, D->Files.Num() - 1);
	if (!D->Files.IsValidIndex(Var) || D->Files[Var].IsEmpty()) return;
	if (D->Cat == TEXT("voice"))
	{
		// one voice at a time per speaker: a new line cuts the previous one
		if (int32* Prev = VoiceByUnit.Find(E.UnitId)) HLAudio::Stop(*Prev, 0.08f);
		const int32 H = Play(*D, Var, D->Vol * E.Vol * Gain, Pan, 1, 0);
		if (H) { VoiceByUnit.Add(E.UnitId, H); ++Plays; }
		return;
	}
	if (Play(*D, Var, D->Vol * E.Vol * Gain, Pan, 1 + FMath::FRandRange(-D->Pitch, D->Pitch), 0)) ++Plays;
}

void FHLSfx::WantLoop(const FString& Key, const FString& Id, float Vol, float Pan)
{
	const FHLSfxDef* D = Defs.Find(Id);
	if (!D || !D->bLoop) return;
	FLoop& L = Loops.FindOrAdd(Key);
	if (L.Id != Id && L.Handle) { HLAudio::Stop(L.Handle, 0.3f); L.Handle = 0; }
	L.Id = Id; L.bWanted = true;
	if (!L.Handle) { L.Handle = Play(*D, 0, Vol, Pan, 1, 0.4f); L.Vol = Vol; L.Pan = Pan; return; }
	if (FMath::Abs(Vol - L.Vol) > 0.015f || FMath::Abs(Pan - L.Pan) > 0.04f) { HLAudio::Set(L.Handle, Vol, Pan, 0.12f); L.Vol = Vol; L.Pan = Pan; }
}

void FHLSfx::WantLoopAt(const FString& Key, const FString& Id, int32 Area, const FVector2D& P, float Z, float VolMul)
{
	const FHLSfxDef* D = Defs.Find(Id);
	if (!D) return;
	float Gain = 0, Pan = 0;
	if (!Spatial(Area, P, Z, D->Range > 0 ? D->Range : 30, Gain, Pan)) return;
	WantLoop(Key, Id, D->Vol * Gain * VolMul, Pan);
}

FString FHLSfx::AmbienceFor(FHLGame& G) const
{
	const FHLUnit* P = G.PlayerUnit();
	if (!G.bInWorld || !P) return FString();
	if (G.Fight.Stage == 1 && P->Area == 0 && FVector2D::Distance(P->Pos, G.Fight.RingPos) < 45) return TEXT("amb_fightnight");
	const FString& Sz = G.CurSubzone;
	if (G.Areas[P->Area].Def->bDungeon)
	{
		if (Sz == TEXT("Forecourt")) return TEXT("amb_hivewall");
		if (Sz == TEXT("Lobby")) return TEXT("amb_lobby");
		if (Sz == TEXT("Stage")) return TEXT("amb_stage");
		if (Sz == TEXT("Auditorium")) return TEXT("amb_hive");
		return TEXT("amb_dungeon");
	}
	const int32 Hour = FDateTime::Now().GetHour();
	const bool bNight = Hour >= 20 || Hour < 6;
	if (Sz == TEXT("The Hive Wall")) return TEXT("amb_hivewall");
	if (Sz == TEXT("Hollywood Police Station")) return TEXT("amb_police");
	if (Sz == TEXT("Hollywood Hotel Ballroom")) return TEXT("amb_ballroom");
	if (Sz == TEXT("Selma Avenue Alleys")) return TEXT("amb_alley");
	if (Sz == TEXT("Cahuenga Rooftops")) return TEXT("amb_rooftops");
	if (Sz == TEXT("Cahuenga Pass")) return TEXT("amb_pass");
	if (Sz == TEXT("Franklin Slopes") || Sz == TEXT("Gower Gulch")) return bNight ? TEXT("amb_hills_night") : TEXT("amb_hills_day");
	if (Sz == TEXT("Taft Back Lots")) return bNight ? TEXT("amb_alley") : TEXT("amb_backlot");
	if (Sz == TEXT("Grauman's Chinese Forecourt")) return bNight ? TEXT("amb_boulevard_night") : TEXT("amb_hub");
	return bNight ? TEXT("amb_boulevard_night") : TEXT("amb_boulevard_day");
}

void FHLSfx::TickSteps(FHLGame& G, float Dt)
{
	const FHLUnit* P = G.PlayerUnit();
	if (!P || !G.bInWorld || P->bDead) { bWasAir = false; return; }
	const bool bAir = P->Z > 0.01f || P->VelZ > 0;
	const int32 Sex = G.Player.Look.Sex & 1;
	if (bAir && !bWasAir && P->VelZ > 0 && G.Chance(0.6f)) G.Sound(Sex ? TEXT("pl_jump_f") : TEXT("pl_jump_m"), *P);
	if (!bAir && bWasAir) G.Sound(TEXT("pl_land"), *P);
	bWasAir = bAir;
	if (bAir || !P->bMovedThisTick || P->bMounted || P->bGhost) { StepT = FMath::Min(StepT, 0.1f); return; }
	StepT -= Dt * FMath::Max(0.5f, P->Speed * P->MoveMult / 7.f);
	if (StepT > 0) return;
	StepT = 0.34f;
	const FString& Sz = G.CurSubzone;
	const TCHAR* Step = TEXT("step_street");
	if (G.Areas[P->Area].Def->bDungeon) Step = Sz == TEXT("Lobby") || Sz == TEXT("Auditorium") ? TEXT("step_carpet") : Sz == TEXT("Stage") || Sz == TEXT("Projection Booth") ? TEXT("step_wood") : TEXT("step_stone");
	else if (Sz == TEXT("The Hive Wall")) Step = TEXT("step_hive");
	else if (Sz == TEXT("Franklin Slopes") || Sz == TEXT("Gower Gulch") || Sz == TEXT("Cahuenga Pass") || Sz == TEXT("Taft Back Lots")) Step = TEXT("step_dirt");
	else if (Sz == TEXT("Hollywood Hotel Ballroom")) Step = TEXT("step_wood");
	G.Sound(Step, *P);
}

void FHLSfx::TickLoops(FHLGame& G, float Dt)
{
	for (auto& KV : Loops) KV.Value.bWanted = false;
	const FHLUnit* P = G.PlayerUnit();
	if (G.bInWorld && P)
	{
		// casting, wings, burrowing, skittering (nearest few of each)
		int32 Wings = 0, Skitter = 0;
		for (const FHLUnit* U : G.UnitList)
		{
			if (U->bDead || U->Area != LArea || FVector2D::DistSquared(U->Pos, LPos) > 50.f * 50.f) continue;
			if (U->Casting)
			{
				const FString& S = U->Casting->School;
				const TCHAR* Id = S == TEXT("electric") ? TEXT("cast_electric") : S == TEXT("fire") ? TEXT("cast_fire") : S == TEXT("holy") ? TEXT("cast_holy")
					: S == TEXT("heal") ? TEXT("cast_heal") : S == TEXT("acid") ? (U->Kind == EHLKind::Mob ? TEXT("cast_bug") : TEXT("cast_acid")) : TEXT("cast_aim");
				WantLoopAt(FString::Printf(TEXT("c%d"), U->Id), Id, U->Area, U->Pos, U->Z + 1, U->Id == G.PlayerUnitId ? 1.6f : 1.f);
			}
			if (!U->Mob) continue;
			const FString& Fam = U->Mob->Family;
			if ((Fam == TEXT("invader_drone") || Fam == TEXT("invader_flier")) && Wings < 4) { ++Wings; WantLoopAt(FString::Printf(TEXT("w%d"), U->Id), TEXT("bug_wings"), U->Area, U->Pos, U->Z + U->Lift + 1); }
			else if (U->bBurrowed && U->MobState == EHLMobState::Combat) WantLoopAt(FString::Printf(TEXT("b%d"), U->Id), TEXT("burrow_dig"), U->Area, U->Pos, 0);
			else if (U->Shape == TEXT("bug") && U->bMovedThisTick && Skitter < 3) { ++Skitter; WantLoopAt(FString::Printf(TEXT("k%d"), U->Id), TEXT("bug_skitter"), U->Area, U->Pos, U->Z); }
		}
		// pulsing hive objects
		if (G.Areas.IsValidIndex(LArea))
			for (int32 i = 0; i < G.Areas[LArea].Objects.Num(); ++i)
			{
				const FHLObjectState& O = G.Areas[LArea].Objects[i];
				if (!O.bActive) continue;
				if (O.Def->Kind == TEXT("spire")) WantLoopAt(FString::Printf(TEXT("o%d"), i), TEXT("loop_spire"), LArea, O.Def->Pos, 2);
				else if (O.Def->Kind == TEXT("egg_sac") || O.Def->Kind == TEXT("boss_egg")) WantLoopAt(FString::Printf(TEXT("o%d"), i), TEXT("loop_eggsac"), LArea, O.Def->Pos, 1);
			}
		// the gunsmith's bench
		if (G.CraftIdx >= 0 && G.Now < G.CraftEnd) WantLoop(TEXT("craft"), TEXT("craft_work"), Defs.Contains(TEXT("craft_work")) ? Defs[TEXT("craft_work")].Vol : 0.4f, 0);
	}
	for (auto It = Loops.CreateIterator(); It; ++It)
		if (!It.Value().bWanted) { if (It.Value().Handle) HLAudio::Stop(It.Value().Handle, 0.5f); It.RemoveCurrent(); }

	// ambience bed: crossfade on change (the native decoder may need a few frames for a new bed: retried until it starts)
	AmbWant = AmbienceFor(G);
	if (AmbWant != AmbId)
	{
		const FHLSfxDef* D = Defs.Find(AmbWant);
		const int32 H = D ? Play(*D, 0, D->Vol, 0, 1, 3.f) : 0;
		if (H || !D)
		{
			if (AmbHandle) HLAudio::Stop(AmbHandle, 3.f);
			AmbHandle = H; AmbId = AmbWant;
		}
	}
}

void FHLSfx::Tick(FHLGame& G, float Dt, int32 Area, const FVector2D& Pos, float Z, float YawDeg)
{
	Clock += Dt;
	LArea = Area; LPos = Pos; LZ = Z; LYaw = YawDeg;
	if (!bReady) { G.SoundQueue.Reset(); return; }
	if (Volume > 0)
	{
		TickSteps(G, Dt);
		for (const FHLSoundEvent& E : G.SoundQueue) Event(G, E);
		TickLoops(G, Dt);
	}
	else if (Loops.Num() || AmbHandle) StopAll();
	G.SoundQueue.Reset();
	ThisFrame.Reset();
	if (LastByUnit.Num() > 2000) LastByUnit.Reset();
	if (Clock >= NextStats)
	{
		NextStats = Clock + 20;
		HLBridge::Log(FString::Printf(TEXT("HL_SFX %d plays, %d loops, bed %s %s"), Plays, Loops.Num(), AmbId.IsEmpty() ? TEXT("-") : *AmbId, *HLAudio::Stats()));
	}
	HLAudio::Flush();
}

// ------------------------------------------------------------------------------------------------- gameplay sound helpers

static bool IsFemale(const FHLGame& G, const FHLUnit& U)
{
	if (U.Id == G.PlayerUnitId) return (G.Player.Look.Sex & 1) == 1;
	if (U.SimIdx >= 0 && G.Sims.IsValidIndex(U.SimIdx))
	{
		const FHLSim& S = G.Sims[U.SimIdx];
		return (FHLAppearance::FromSeed(GetTypeHash(S.P ? S.P->Id : U.Name), S.P ? S.P->Body : -1).Sex & 1) == 1;
	}
	return false;
}

FString FHLGame::SfxVoice(const FHLUnit& U) const
{
	if (U.Mob)
	{
		if (U.Mob->Family == TEXT("dragna_enforcer")) return TEXT("enforcer");
		return U.Mob->Family;
	}
	return FString();
}

const FHLItem* FHLGame::SfxWeapon(const FHLUnit& U, bool bRanged) const
{
	const TCHAR* Slot = bRanged ? TEXT("ranged") : TEXT("main");
	if (U.Id == PlayerUnitId) return Player.Equip.FindRef(Slot);
	if (U.SimIdx >= 0 && Sims.IsValidIndex(U.SimIdx)) return Sims[U.SimIdx].Gear.FindRef(Slot);
	return nullptr;
}

void FHLGame::SfxAttack(const FHLUnit& U, const FHLUnit& T, int32 Outcome)
{
	// Outcome: 0 hit, 1 crit, 2 miss, 3 dodge, 4 parry, 5 block
	const FHLItem* W = SfxWeapon(U, U.bRangedAuto);
	const FString Type = W ? W->Type : FString();
	const bool bBug = U.Shape == TEXT("bug"), bBeast = U.Shape == TEXT("quad");
	FString Hit = TEXT("hit_blunt");
	if (U.bRangedAuto)
	{
		const TCHAR* Shot = Type == TEXT("rifle") ? TEXT("shot_rifle") : Type == TEXT("shotgun") ? TEXT("shot_shotgun") : Type == TEXT("smg") ? TEXT("shot_smg")
			: Type == TEXT("pistol") || U.Mob ? TEXT("shot_pistol") : TEXT("shot_revolver");
		Sound(Shot, U);
		Hit = TEXT("hit_bullet");
	}
	else if (bBug || bBeast) { Hit = TEXT("hit_blade"); if (Chance(0.35f)) Sound(SfxVoice(U) + TEXT("_attack"), U); }
	else
	{
		const bool bLight = Type == TEXT("knife") || Type == TEXT("sabre") || Type == TEXT("fist") || Type.IsEmpty();
		Sound(bLight ? TEXT("swing_light") : TEXT("swing_heavy"), U, 0.8f);
		Hit = Type == TEXT("knife") || Type == TEXT("sabre") ? TEXT("hit_blade") : (Type == TEXT("fist") || (Type.IsEmpty() && !U.Mob)) ? TEXT("hit_fist") : TEXT("hit_blunt");
		if (U.Mob && Chance(0.25f)) Sound(SfxVoice(U) + TEXT("_attack"), U);
	}
	if (!U.bRangedAuto && T.Shape == TEXT("bug")) Hit = TEXT("hit_bug");
	switch (Outcome)
	{
	case 0: Sound(Hit, T); break;
	case 1: Sound(Hit, T); Sound(TEXT("hit_crit"), T); break;
	case 2: if (!U.bRangedAuto) Sound(TEXT("miss"), T); break;
	case 3: Sound(TEXT("dodge"), T); break;
	case 4: Sound(TEXT("parry"), T); break;
	case 5: Sound(TEXT("block"), T); break;
	}
}

void FHLGame::SfxHurt(const FHLUnit& U)
{
	if (U.Mob) { if (Chance(0.4f)) Sound(SfxVoice(U) + TEXT("_hurt"), U); return; }
	if (U.Kind == EHLKind::Player || U.Kind == EHLKind::Sim || U.Kind == EHLKind::Npc)
		if (Chance(U.Kind == EHLKind::Player ? 0.55f : 0.3f)) Sound(IsFemale(*this, U) ? TEXT("pl_hurt_f") : TEXT("pl_hurt_m"), U);
}

void FHLGame::SfxDeath(const FHLUnit& U)
{
	if (U.Mob) { Sound(SfxVoice(U) + TEXT("_death"), U); return; }
	Sound(IsFemale(*this, U) ? TEXT("pl_death_f") : TEXT("pl_death_m"), U);
}

void FHLGame::SfxObject(const FHLObjectDef& Def, bool bStart)
{
	const FString& K = Def.Kind;
	const TCHAR* Id = nullptr;
	if (bStart)
	{
		if (K == TEXT("scrap")) Id = TEXT("obj_scrap");
		else if (K == TEXT("spire")) Id = TEXT("obj_spire_charge");
		else if (K == TEXT("mailbox")) Id = TEXT("obj_mailbox");
		else if (K == TEXT("redcar")) Id = TEXT("obj_redcar_bell");
	}
	else
	{
		if (K == TEXT("coffee_tin")) Id = TEXT("obj_tin");
		else if (K == TEXT("egg") || K == TEXT("spread_egg") || K == TEXT("boss_egg") || K == TEXT("egg_sac")) Id = TEXT("obj_egg");
		else if (K == TEXT("film_can")) Id = TEXT("obj_film");
		else if (K == TEXT("callbox")) Id = TEXT("obj_callbox");
		else if (K == TEXT("arc_lamp")) Id = TEXT("obj_arclamp");
		else if (K == TEXT("reel")) Id = TEXT("obj_reel");
		else if (K == TEXT("usher")) Id = TEXT("obj_usher");
		else if (K == TEXT("spire")) Id = TEXT("obj_spire_blast");
		else if (K == TEXT("scrap")) Id = TEXT("ui_loot");
		else Id = TEXT("ui_loot");
	}
	if (!Id) return;
	const FHLUnit* P = PlayerUnit();
	SoundAt(Id, P ? P->Area : 0, Def.Pos);
}
