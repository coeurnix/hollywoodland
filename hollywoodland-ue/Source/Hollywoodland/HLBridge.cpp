#include "HLBridge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_WASM
#include <atomic>
#include <stdlib.h>
#include <string.h>
#include <emscripten/em_js.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

namespace
{
	struct alignas(16) FSlot
	{
		std::atomic<int32> State; // 0 pending, 1 done (Atomics.store from main-thread JS)
		int32 Pad;
		double ResultAddr;        // char* JSON {status, err, body}, malloc'd by JS, freed here
	};
	struct FStartArgs { FSlot* Slot; int32 Op; double TimeoutMs; char* A; char* B; char* C; };
	struct FPending { FSlot* Slot; FHLBridgeCallback Done; };
	TArray<FPending> GPending;

	char* DupUtf8(const FString& S)
	{
		FTCHARToUTF8 Conv(*S);
		char* P = (char*)malloc(Conv.Length() + 1);
		memcpy(P, Conv.Get(), Conv.Length());
		P[Conv.Length()] = 0;
		return P;
	}
}

// Main thread. Op 0: fetch(A=method, B=url, C=body). Op 1: localStorage.getItem(A). Op 2: localStorage.setItem(A, B). Op 3: console.log(A).
// Pointers cross as doubles (wasm64: pointer args may arrive as BigInt).
EM_JS(void, HL_JsStart, (double Slot, int Op, double TimeoutMs, double A, double B, double C), {
	var a = A ? UTF8ToString(A) : "";
	var b = B ? UTF8ToString(B) : "";
	var c = C ? UTF8ToString(C) : "";
	function publish(rec) {
		if (!Slot) return;
		try {
			var addr = stringToNewUTF8(JSON.stringify(rec));
			new Float64Array(HEAP8.buffer, Slot + 8, 1)[0] = Number(addr);
			Atomics.store(new Int32Array(HEAP8.buffer, Slot, 1), 0, 1);
		} catch (x) { console.error("HL bridge publish failed: " + x); }
	}
	if (Op === 3) { console.log(a); return; }
	if (Op === 4 || Op === 5) {
		// Music. Op 4: play A (url, "" = stop), B = "loop,fadeMs,volume". Op 5: volume A.
		var M = window.__hlMusic;
		if (!M) {
			M = window.__hlMusic = { cur: null, vol: 0.5 };
			M.ramp = function (el, to, ms, done) {
				if (el.__t) clearInterval(el.__t);
				var from = el.volume, t0 = performance.now();
				if (ms <= 0) { el.volume = to; if (done) done(); return; }
				el.__t = setInterval(function () {
					var k = Math.min(1, (performance.now() - t0) / ms);
					el.volume = Math.max(0, Math.min(1, from + (to - from) * k));
					if (k >= 1) { clearInterval(el.__t); el.__t = 0; if (done) done(); }
				}, 40);
			};
			M.start = function (el) {
				var p = el.play();
				if (p && p.then) p.then(function () { console.log("HL music playing " + el.__name); },
					function () { el.__blocked = true; console.log("HL music waiting for a click: " + el.__name); });
			};
			M.unlock = function () { var el = M.cur; if (el && el.__blocked && el.paused) { el.__blocked = false; M.start(el); } };
			document.addEventListener("pointerdown", M.unlock, true);
			document.addEventListener("keydown", M.unlock, true);
		}
		if (Op === 5) { M.vol = Math.max(0, Math.min(1, parseFloat(a) || 0)); if (M.cur) M.ramp(M.cur, M.vol, 300); return; }
		var parts = b.split(","), loop = parts[0] === "1", fade = parseFloat(parts[1]) || 0;
		var v = parseFloat(parts[2]);
		if (v >= 0) M.vol = Math.min(1, v);
		var old = M.cur, src = a.trim();
		M.cur = null;
		if (old) M.ramp(old, 0, fade, function () { old.pause(); old.removeAttribute("src"); old.load(); });
		if (!src) return;
		a = src;
		var el = new Audio();
		el.__name = a; el.loop = loop; el.preload = "auto"; el.volume = 0;
		el.src = new URL(a, location.href).href;
		M.cur = el;
		M.start(el);
		M.ramp(el, M.vol, fade);
		return;
	}
	if (Op === 1) {
		var v = null;
		try { v = localStorage.getItem(a); } catch (e) {}
		publish({ status: v === null ? 404 : 200, err: "", body: v === null ? "" : v });
		return;
	}
	if (Op === 2) {
		try { localStorage.setItem(a, b); } catch (e) { console.warn("HL storage set failed: " + e); }
		return;
	}
	var rec = { status: 0, err: "", body: "" };
	var init = { method: a, cache: "no-store", mode: "cors", credentials: "omit" };
	if (c && a !== "GET") { init.body = c; init.headers = { "Content-Type": "text/plain" }; }
	var ctl = new AbortController();
	init.signal = ctl.signal;
	var timer = setTimeout(function () { ctl.abort(); }, TimeoutMs);
	fetch(new URL(b, location.href).href, init).then(function (r) {
		rec.status = r.status;
		return r.text().then(function (t) { rec.body = t; });
	}).catch(function (e) {
		rec.status = 0; rec.err = String((e && e.message) || e);
	}).then(function () { clearTimeout(timer); publish(rec); });
});

static void StartOp(int32 Op, const FString& A, const FString& B, const FString& C, FHLBridgeCallback Done, float TimeoutS)
{
	FSlot* Slot = nullptr;
	if (Done)
	{
		Slot = (FSlot*)aligned_alloc(16, sizeof(FSlot));
		new (&Slot->State) std::atomic<int32>(0);
		Slot->ResultAddr = 0;
		GPending.Add({ Slot, MoveTemp(Done) });
	}
	FStartArgs* Args = new FStartArgs{ Slot, Op, TimeoutS * 1000.0, DupUtf8(A), B.IsEmpty() ? nullptr : DupUtf8(B), C.IsEmpty() ? nullptr : DupUtf8(C) };
	emscripten_proxy_async(emscripten_proxy_get_system_queue(), emscripten_main_runtime_thread_id(), [](void* Arg)
	{
		FStartArgs* S = static_cast<FStartArgs*>(Arg);
		HL_JsStart(double(UPTRINT(S->Slot)), S->Op, S->TimeoutMs, double(UPTRINT(S->A)), double(UPTRINT(S->B)), double(UPTRINT(S->C)));
		free(S->A); free(S->B); free(S->C);
		delete S;
	}, Args);
}

void HLBridge::Fetch(const FString& Method, const FString& Url, const FString& Body, FHLBridgeCallback Done, float TimeoutS)
{
	StartOp(0, Method, Url, Body, MoveTemp(Done), TimeoutS);
}
void HLBridge::StorageGet(const FString& Key, FHLBridgeCallback Done) { StartOp(1, Key, FString(), FString(), MoveTemp(Done), 0); }
void HLBridge::StorageSet(const FString& Key, const FString& Value) { StartOp(2, Key, Value, FString(), nullptr, 0); }
void HLBridge::Log(const FString& Line) { StartOp(3, Line, FString(), FString(), nullptr, 0); }
void HLBridge::MusicPlay(const FString& File, bool bLoop, float FadeS, float Volume)
{
	StartOp(4, File.IsEmpty() ? FString(TEXT(" ")) : File, FString::Printf(TEXT("%d,%d,%.3f"), bLoop ? 1 : 0, (int32)(FadeS * 1000), Volume), FString(), nullptr, 0);
}
void HLBridge::MusicStop(float FadeS) { StartOp(4, FString(TEXT(" ")), FString::Printf(TEXT("0,%d,-1"), (int32)(FadeS * 1000)), FString(), nullptr, 0); }
void HLBridge::MusicVolume(float Volume) { StartOp(5, FString::Printf(TEXT("%.3f"), Volume), FString(), FString(), nullptr, 0); }
void HLBridge::MusicTick(float) {}

void HLBridge::Poll()
{
	for (int32 i = 0; i < GPending.Num();)
	{
		FSlot* Slot = GPending[i].Slot;
		if (Slot->State.load(std::memory_order_acquire) != 1) { ++i; continue; }
		char* Json = reinterpret_cast<char*>(static_cast<UPTRINT>(Slot->ResultAddr));
		FHLBridgeResult R;
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(Json)));
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
		{
			R.Status = (int32)Obj->GetNumberField(TEXT("status"));
			Obj->TryGetStringField(TEXT("err"), R.Error);
			Obj->TryGetStringField(TEXT("body"), R.Body);
		}
		else { R.Error = TEXT("bad bridge json"); }
		free(Json);
		free(Slot);
		FHLBridgeCallback Done = MoveTemp(GPending[i].Done);
		GPending.RemoveAt(i);
		Done(R);
	}
}

#else

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

namespace
{
	TArray<TPair<FHLBridgeCallback, FHLBridgeResult>> GDone;
	FString StoragePath(const FString& Key) { return FPaths::ProjectSavedDir() / TEXT("HLStorage") / (Key + TEXT(".json")); }
}

void HLBridge::Fetch(const FString& Method, const FString& Url, const FString& Body, FHLBridgeCallback Done, float TimeoutS)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetVerb(Method);
	Req->SetURL(Url);
	Req->SetTimeout(TimeoutS);
	if (!Body.IsEmpty()) { Req->SetContentAsString(Body); Req->SetHeader(TEXT("Content-Type"), TEXT("text/plain")); }
	Req->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		FHLBridgeResult R;
		if (bOk && Resp.IsValid()) { R.Status = Resp->GetResponseCode(); R.Body = Resp->GetContentAsString(); }
		else { R.Error = TEXT("request failed"); }
		GDone.Add({ Done, R });
	});
	Req->ProcessRequest();
}
void HLBridge::StorageGet(const FString& Key, FHLBridgeCallback Done)
{
	FHLBridgeResult R;
	R.Status = FFileHelper::LoadFileToString(R.Body, *StoragePath(Key)) ? 200 : 404;
	GDone.Add({ Done, R });
}
void HLBridge::StorageSet(const FString& Key, const FString& Value) { FFileHelper::SaveStringToFile(Value, *StoragePath(Key)); }
void HLBridge::Log(const FString& Line) { UE_LOG(LogTemp, Display, TEXT("%s"), *Line); }

// Native music: Windows MCI (DirectShow MP3 decode), two aliases for crossfades. Volume 0..1000 per alias.
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmsystem.h>
#include "Windows/HideWindowsPlatformTypes.h"
namespace
{
	struct FMciVoice { FString Alias; float Vol = 0, Target = 0, Rate = 0; bool bOpen = false, bCloseAtZero = false; };
	FMciVoice GVoices[2] = { { TEXT("hlm0") }, { TEXT("hlm1") } };
	int32 GCur = -1;
	float GMaster = 0.5f;
	void Mci(const FString& Cmd)
	{
		const MCIERROR Err = mciSendStringW(*Cmd, nullptr, 0, nullptr);
		if (Err && !Cmd.StartsWith(TEXT("setaudio")))
		{
			TCHAR Msg[256] = { 0 };
			mciGetErrorStringW(Err, Msg, 255);
			UE_LOG(LogTemp, Warning, TEXT("HL music MCI '%s' failed: %s"), *Cmd, Msg);
		}
	}
	void SetVol(FMciVoice& V) { Mci(FString::Printf(TEXT("setaudio %s volume to %d"), *V.Alias, FMath::Clamp((int32)(V.Vol * 1000), 0, 1000))); }
	void FadeTo(FMciVoice& V, float Target, float FadeS, bool bClose)
	{
		V.Target = Target; V.bCloseAtZero = bClose;
		V.Rate = FadeS > 0.01f ? FMath::Max(0.01f, FMath::Abs(Target - V.Vol)) / FadeS : 1000.f;
	}
}
void HLBridge::MusicPlay(const FString& File, bool bLoop, float FadeS, float Volume)
{
	if (Volume >= 0) GMaster = FMath::Clamp(Volume, 0.f, 1.f);
	if (GCur >= 0) FadeTo(GVoices[GCur], 0, FadeS, true);
	if (File.IsEmpty()) { GCur = -1; return; }
	const int32 Next = GCur == 0 ? 1 : 0;
	FMciVoice& V = GVoices[Next];
	if (V.bOpen) { Mci(TEXT("close ") + V.Alias); V.bOpen = false; }
	Mci(FString::Printf(TEXT("open \"%s\" type mpegvideo alias %s"), *File, *V.Alias));
	V.bOpen = true; V.Vol = 0; SetVol(V);
	Mci(FString::Printf(TEXT("play %s%s"), *V.Alias, bLoop ? TEXT(" repeat") : TEXT("")));
	FadeTo(V, GMaster, FadeS, false);
	GCur = Next;
}
void HLBridge::MusicStop(float FadeS) { MusicPlay(FString(), false, FadeS, -1); }
void HLBridge::MusicVolume(float Volume) { GMaster = FMath::Clamp(Volume, 0.f, 1.f); if (GCur >= 0) FadeTo(GVoices[GCur], GMaster, 0.3f, false); }
void HLBridge::MusicTick(float Dt)
{
	for (FMciVoice& V : GVoices)
	{
		if (!V.bOpen || V.Vol == V.Target) continue;
		V.Vol = V.Vol < V.Target ? FMath::Min(V.Target, V.Vol + V.Rate * Dt) : FMath::Max(V.Target, V.Vol - V.Rate * Dt);
		SetVol(V);
		if (V.Vol <= 0 && V.bCloseAtZero) { Mci(TEXT("close ") + V.Alias); V.bOpen = false; }
	}
}
#else
void HLBridge::MusicPlay(const FString&, bool, float, float) {}
void HLBridge::MusicStop(float) {}
void HLBridge::MusicVolume(float) {}
void HLBridge::MusicTick(float) {}
#endif
void HLBridge::Poll()
{
	TArray<TPair<FHLBridgeCallback, FHLBridgeResult>> Done = MoveTemp(GDone);
	GDone.Reset();
	for (auto& P : Done) { if (P.Key) P.Key(P.Value); }
}
#endif
