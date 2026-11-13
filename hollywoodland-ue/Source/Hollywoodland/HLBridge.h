// Browser bridge: HTTP (fetch), persistent key/value storage (localStorage) and console logging.
// Web: FHttpModule is null on Wasm and the game thread never returns to its JS event loop, so every op is proxied to the
// browser main thread (emscripten_proxy_async) and its result is published into a slot the game thread polls.
// Native (editor -game): FHttpModule + files under Saved/HLStorage.
#pragma once
#include "CoreMinimal.h"

struct FHLBridgeResult
{
	int32 Status = 0;   // HTTP status; 200/404 for storage ops; 0 on network error
	FString Body;
	FString Error;
	bool Ok() const { return Status >= 200 && Status < 300; }
};
typedef TFunction<void(const FHLBridgeResult&)> FHLBridgeCallback;

namespace HLBridge
{
	/** Async HTTP. Body is sent as text/plain (no CORS preflight). */
	void Fetch(const FString& Method, const FString& Url, const FString& Body, FHLBridgeCallback Done, float TimeoutS = 8.f);
	void StorageGet(const FString& Key, FHLBridgeCallback Done);
	void StorageSet(const FString& Key, const FString& Value);
	/** Dispatch finished callbacks. Call once per frame on the game thread. */
	void Poll();
	/** console.log on the web (works in Shipping); UE_LOG natively. */
	void Log(const FString& Line);
	// web: prefix for music/ and sfx/ URLs; empty (relative to the page) unless [Hollywoodland] AudioBase in Engine.ini names
	// another host. The itch package carries the audio (sfx bundled, see tools/itch/audiopack.mjs); Cloudflare serves R2 prefixes
	FString AudioBase();

	/** Streaming music, one track at a time with crossfades. Web: an <audio> element on the page (File is a URL relative to the
	 *  page, played once the browser allows audio). Native: Windows MCI on an MP3 path. Volume 0..1. */
	void MusicPlay(const FString& File, bool bLoop, float FadeS, float Volume);
	void MusicStop(float FadeS);
	void MusicVolume(float Volume);
	/** Native volume ramps; no-op on the web (ramps run in JS). Call once per frame. */
	void MusicTick(float Dt);
	/** Web only: one batch of sound-effect commands for the page's WebAudio player (HLSfx.cpp builds them). */
	void Audio(const FString& Commands);
}
