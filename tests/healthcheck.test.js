const assert = require("node:assert/strict");
const test = require("node:test");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const { EventEmitter } = require("node:events");

const scriptDirectory = path.join(__dirname, "..", "scripts");
const diagnostic = fs.readFileSync(path.join(scriptDirectory, "healthcheck.js"), "utf8");

// Run the public diagnostic with other prerequisites valid. Only built UI
// availability and HTTP responses vary; no subprocesses or services are started.
function runDiagnostic({ builtUI = true, legacyUI = false, httpStatus = 200 } = {}) {
  return new Promise((resolve, reject) => {
    const modules = {
      path,
      fs: { existsSync(filename) {
        const relative = path.relative(path.join(scriptDirectory, ".."), filename);
        if (relative === path.join("ui", "dist", "index.html")) return builtUI;
        if (relative === "apm-dashboard.html" || relative === path.join("ui", "apm-dashboard.html")) return legacyUI;
        return true;
      } },
      child_process: { execSync: () => "Available prerequisite" },
      http: { get(url, options, callback) {
        const request = new EventEmitter();
        request.destroy = () => {};
        queueMicrotask(() => {
          const response = new EventEmitter();
          response.statusCode = httpStatus;
          callback(response);
          response.emit("data", url.endsWith("/health") ? '{"ok":true}' : "<!doctype html><html></html>");
          response.emit("end");
        });
        return request;
      } }
    };
    try {
      vm.runInNewContext(diagnostic, {
        __dirname: scriptDirectory,
        require(name) {
          if (!(name in modules)) throw new Error(`Unexpected diagnostic dependency: ${name}`);
          return modules[name];
        },
        process: { version: process.version, env: {}, stdout: { write() {} }, exit: resolve },
        console: { log() {}, error() {} }
      }, { filename: "healthcheck.js" });
    } catch (error) {
      reject(error);
    }
  });
}

test("a built dashboard and healthy services pass the documented diagnostic", async () => {
  assert.equal(await runDiagnostic(), 0);
});

test("a legacy HTML file cannot replace the current built dashboard", async () => {
  assert.equal(await runDiagnostic({ builtUI: false, legacyUI: true }), 1);
});

test("a missing built dashboard fails the diagnostic", async () => {
  assert.equal(await runDiagnostic({ builtUI: false }), 1);
});

test("failed runtime HTTP checks cannot produce a successful diagnostic", async () => {
  assert.equal(await runDiagnostic({ httpStatus: 503 }), 1);
});
