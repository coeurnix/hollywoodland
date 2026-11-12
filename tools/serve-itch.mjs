// Serves itch/index.html the way itch.io does with "SharedArrayBuffer support" on (COOP + COEP), to test the nested iframe.
// usage: node tools/serve-itch.mjs [port]   then open http://localhost:8787/
import { createServer } from 'node:http';
import { readFileSync } from 'node:fs';
const port = Number(process.argv[2] || 8787);
createServer((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cross-Origin-Opener-Policy': 'same-origin', 'Cross-Origin-Embedder-Policy': 'require-corp' });
  res.end(readFileSync(new URL('../itch/index.html', import.meta.url)));
}).listen(port, () => console.log(`http://localhost:${port}/`));
