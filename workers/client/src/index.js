// hollywoodland: serves the UE WebAssembly stage from R2 (logic from Engine/Platforms/Wasm/Build/Tools/deploy/worker.js).
// One stage per R2 prefix; env.DEPLOY names the live one. Differences from Epic's worker: Cross-Origin-Resource-Policy is
// "cross-origin" so the page can be embedded in the itch.io iframe (which is cross-origin isolated by itch's
// "SharedArrayBuffer support" option; the embedding iframe carries allow="cross-origin-isolated").
// Whole-file GET -> the .br sibling with Content-Encoding br (encodeBody manual). Range GET -> raw bytes, 206 (IoStore reads).

const TYPES = { wasm: 'application/wasm', js: 'text/javascript', mjs: 'text/javascript', json: 'application/json',
  webmanifest: 'application/manifest+json', html: 'text/html; charset=utf-8', css: 'text/css', png: 'image/png',
  jpg: 'image/jpeg', webp: 'image/webp', svg: 'image/svg+xml', txt: 'text/plain; charset=utf-8' };

function baseHeaders(path) {
  const h = new Headers();
  const ext = path.split('.').pop().toLowerCase();
  h.set('Content-Type', TYPES[ext] || 'application/octet-stream');
  h.set('Cross-Origin-Opener-Policy', 'same-origin');
  h.set('Cross-Origin-Embedder-Policy', 'require-corp');
  h.set('Cross-Origin-Resource-Policy', 'cross-origin');
  h.set('Cache-Control', 'no-cache, no-transform');
  h.set('X-Content-Type-Options', 'nosniff');
  h.set('Accept-Ranges', 'bytes');
  return h;
}

export default {
  async fetch(request, env) {
    if (request.method !== 'GET' && request.method !== 'HEAD') return new Response(null, { status: 405 });
    const url = new URL(request.url);
    // the engine's fetch backend asks for <dir>//<name>: one URL per file
    let path = decodeURIComponent(url.pathname).replace(/\/{2,}/g, '/').replace(/^\//, '');
    if (path === '' || path.endsWith('/')) path += 'index.html';
    if (path === '_headers' || path.endsWith('.br')) return new Response(null, { status: 404 });
    if (path === 'favicon.ico') return new Response(null, { status: 204, headers: { 'Cache-Control': 'max-age=86400' } });
    const key = `${env.DEPLOY}/${path}`;
    const headers = baseHeaders(path);
    const range = request.headers.get('Range');
    const acceptsBr = /\bbr\b/.test(request.headers.get('Accept-Encoding') || '');

    if (!range && acceptsBr) {
      const encoded = request.method === 'HEAD' ? await env.BUCKET.head(key + '.br') : await env.BUCKET.get(key + '.br');
      if (encoded) {
        headers.set('Content-Encoding', 'br');
        headers.set('Vary', 'Accept-Encoding');
        headers.set('ETag', encoded.httpEtag);
        if (request.headers.get('If-None-Match') === encoded.httpEtag) return new Response(null, { status: 304, headers });
        return new Response(request.method === 'HEAD' ? null : encoded.body, { status: 200, headers, encodeBody: 'manual' });
      }
    }
    const object = request.method === 'HEAD'
      ? await env.BUCKET.head(key)
      : await env.BUCKET.get(key, range ? { range: request.headers } : {});
    if (!object) return new Response(null, { status: 404, headers });
    headers.set('ETag', object.httpEtag);
    if (!range && request.headers.get('If-None-Match') === object.httpEtag) return new Response(null, { status: 304, headers });
    if (range && object.range) {
      const first = object.range.offset ?? (object.size - object.range.suffix);
      const length = object.range.length ?? (object.size - first);
      headers.set('Content-Range', `bytes ${first}-${first + length - 1}/${object.size}`);
      headers.set('Content-Length', String(length));
      return new Response(object.body, { status: 206, headers });
    }
    headers.set('Content-Length', String(object.size));
    return new Response(request.method === 'HEAD' ? null : object.body, { status: 200, headers });
  },
};
