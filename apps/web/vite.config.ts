import fs from 'node:fs';
import type { Plugin } from 'vite';
import { defineConfig } from 'vitest/config';
import { alias, repoRoot, wasmCommand, wasmPath } from './vite.shared.ts';

// Localhost only: the dev page fetches the wasm the CMake build wrote.
function devWasm(): Plugin {
  return {
    name: 'banana-chords-dev-wasm',
    apply: 'serve',
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        if (req.url?.split('?')[0] !== '/banana-chords.wasm') {
          next();
          return;
        }
        if (!fs.existsSync(wasmPath)) {
          res.statusCode = 404;
          res.end(`build-wasm/banana-chords.wasm is missing. From the repo root:\n  ${wasmCommand}`);
          return;
        }
        res.setHeader('Content-Type', 'application/wasm');
        res.setHeader('Cache-Control', 'no-store');
        res.end(fs.readFileSync(wasmPath));
      });
    },
  };
}

export default defineConfig({
  plugins: [devWasm()],
  resolve: { alias },
  // localhost can be IPv6-only unless the host is set; listen on every loopback.
  server: { host: true, port: 5190, fs: { allow: [repoRoot] } },
  test: {
    include: ['src/**/*.test.ts'],
    environment: 'node',
  },
});
