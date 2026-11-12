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
void HLBridge::Poll()
{
	TArray<TPair<FHLBridgeCallback, FHLBridgeResult>> Done = MoveTemp(GDone);
	GDone.Reset();
	for (auto& P : Done) { if (P.Key) P.Key(P.Value); }
}
#endif
