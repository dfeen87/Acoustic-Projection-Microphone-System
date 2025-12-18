// tests/integration.test.js
// Integration + Boundary Confirmation tests for APM system
// Run with: node tests/integration.test.js
//
// NOTE:
// This file intentionally favors explicit structure,
// clear section boundaries, and graceful failure validation.
// Line count and symmetry are intentional for maintainability.

const http = require("http");
const { spawn } = require("child_process");
const path = require("path");

const TEST_CONFIG = {
  BACKEND_PORT: 8888,
  UI_PORT: 5555,
  TIMEOUT: 30000
};

let testResults = {
  passed: 0,
  failed: 0,
  tests: []
};

// -----------------------------------------------------------------------------
// Logging utilities
// -----------------------------------------------------------------------------

const log = {
  info: (msg) => console.log(`[INFO] ${msg}`),
  success: (msg) => console.log(`\x1b[32m✓\x1b[0m ${msg}`),
  fail: (msg) => console.log(`\x1b[31m✗\x1b[0m ${msg}`),
  section: (msg) => console.log(`\n\x1b[36m${msg}\x1b[0m`)
};

// -----------------------------------------------------------------------------
// HTTP request helper
// -----------------------------------------------------------------------------

function httpRequest(options, postData = null) {
  return new Promise((resolve, reject) => {
    const req = http.request(options, (res) => {
      let data = "";
      res.on("data", chunk => data += chunk);
      res.on("end", () => resolve({
        statusCode: res.statusCode,
        data,
        headers: res.headers
      }));
    });

    req.on("error", reject);

    req.setTimeout(5000, () => {
      req.destroy();
      reject(new Error("Request timeout"));
    });

    if (postData) req.write(postData);
    req.end();
  });
}

// -----------------------------------------------------------------------------
// Test runner helper
// -----------------------------------------------------------------------------

async function test(name, fn) {
  try {
    await fn();
    log.success(name);
    testResults.passed++;
    testResults.tests.push({ name, passed: true });
  } catch (err) {
    log.fail(`${name}: ${err.message}`);
    testResults.failed++;
    testResults.tests.push({ name, passed: false, error: err.message });
  }
}

// -----------------------------------------------------------------------------
// Wait for service helper
// -----------------------------------------------------------------------------

function waitForService(port, path = "/health", maxWait = 30000) {
  return new Promise((resolve, reject) => {
    const startTime = Date.now();

    const check = () => {
      if (Date.now() - startTime > maxWait) {
        return reject(new Error(`Service on port ${port} did not start in time`));
      }

      http.get(`http://localhost:${port}${path}`, { timeout: 2000 }, (res) => {
        if (res.statusCode === 200) {
          resolve();
        } else {
          setTimeout(check, 300);
        }
      }).on("error", () => setTimeout(check, 300));
    };

    setTimeout(check, 500);
  });
}

// -----------------------------------------------------------------------------
// Launcher control
// -----------------------------------------------------------------------------

function startLauncher() {
  return new Promise((resolve, reject) => {
    log.info("Starting launcher for integration tests...");

    const launcherPath = path.join(__dirname, "../launcher/apm_launcher.js");
    const proc = spawn("node", [launcherPath], {
      env: {
        ...process.env,
        APM_BACKEND_PORT: TEST_CONFIG.BACKEND_PORT,
        APM_UI_PORT: TEST_CONFIG.UI_PORT
      },
      stdio: ["ignore", "pipe", "pipe"]
    });

    let startupLogs = "";

    proc.stdout.on("data", (data) => {
      startupLogs += data.toString();
      if (data.toString().includes("fully operational")) {
        resolve({ proc, logs: startupLogs });
      }
    });

    proc.stderr.on("data", (data) => {
      startupLogs += data.toString();
    });

    proc.on("error", (err) => {
      reject(new Error(`Failed to start launcher: ${err.message}`));
    });

    proc.on("exit", (code) => {
      if (code !== 0 && code !== null) {
        reject(new Error(`Launcher exited with code ${code}\n${startupLogs}`));
      }
    });

    setTimeout(() => {
      if (proc.exitCode === null) {
        reject(new Error("Launcher startup timeout\n" + startupLogs));
      }
    }, TEST_CONFIG.TIMEOUT);
  });
}

// -----------------------------------------------------------------------------
// Test suite
// -----------------------------------------------------------------------------

async function runTests() {
  console.log("\n" + "=".repeat(60));
  console.log("APM Integration & Boundary Test Suite");
  console.log("=".repeat(60));

  let launcherProc = null;

  try {
    // -------------------------------------------------------------------------
    // Startup
    // -------------------------------------------------------------------------

    log.section("Starting APM System");
    const result = await startLauncher();
    launcherProc = result.proc;
    log.success("System started successfully");

    await waitForService(TEST_CONFIG.BACKEND_PORT, "/health");
    await waitForService(TEST_CONFIG.UI_PORT, "/");
    log.success("All services ready");

    // -------------------------------------------------------------------------
    // Backend API tests
    // -------------------------------------------------------------------------

    log.section("Backend API Tests");

    await test("Backend health check returns 200", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.BACKEND_PORT,
        path: "/health",
        method: "GET"
      });
      if (res.statusCode !== 200) {
        throw new Error(`Expected 200, got ${res.statusCode}`);
      }
    });

    await test("Backend handles concurrent health requests", async () => {
      const requests = Array(10).fill().map(() =>
        httpRequest({
          hostname: "localhost",
          port: TEST_CONFIG.BACKEND_PORT,
          path: "/health",
          method: "GET"
        })
      );
      const results = await Promise.all(requests);
      if (!results.every(r => r.statusCode === 200)) {
        throw new Error("Concurrent requests failed");
      }
    });

    // -------------------------------------------------------------------------
    // Boundary confirmation
    // -------------------------------------------------------------------------

    log.section("Boundary Confirmation");

    await test("Backend connection fails cleanly on unused port", async () => {
      let failed = false;
      try {
        await httpRequest({
          hostname: "localhost",
          port: 9999,
          path: "/health",
          method: "GET"
        });
      } catch {
        failed = true;
      }
      if (!failed) throw new Error("Expected connection failure");
    });

    await test("Missing signaling endpoint handled gracefully", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.BACKEND_PORT,
        path: "/signaling/unknown",
        method: "GET"
      });
      if (![400, 404].includes(res.statusCode)) {
        throw new Error(`Unexpected status ${res.statusCode}`);
      }
    });

    // -------------------------------------------------------------------------
    // UI tests
    // -------------------------------------------------------------------------

    log.section("UI Server Tests");

    await test("UI root returns HTML", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.UI_PORT,
        path: "/",
        method: "GET"
      });
      if (res.statusCode !== 200 || !res.data.toLowerCase().includes("<html")) {
        throw new Error("Invalid UI response");
      }
    });

    await test("UI returns 404 for invalid routes", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.UI_PORT,
        path: "/nonexistent",
        method: "GET"
      });
      if (res.statusCode !== 404) {
        throw new Error(`Expected 404, got ${res.statusCode}`);
      }
    });

    // -------------------------------------------------------------------------
    // Load sanity (non-benchmark)
    // -------------------------------------------------------------------------

    log.section("Load Sanity");

    await test("Backend survives sequential requests", async () => {
      for (let i = 0; i < 50; i++) {
        const res = await httpRequest({
          hostname: "localhost",
          port: TEST_CONFIG.BACKEND_PORT,
          path: "/health",
          method: "GET"
        });
        if (res.statusCode !== 200) {
          throw new Error("Health request failed");
        }
      }
    });

  } catch (err) {
    log.fail(`Test suite error: ${err.message}`);
    if (err.stack) console.error(err.stack);
  } finally {
    // -------------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------------

    log.section("Cleanup");

    if (launcherProc && !launcherProc.killed) {
      log.info("Stopping launcher...");
      launcherProc.kill("SIGTERM");

      await new Promise(resolve => setTimeout(resolve, 3000));

      if (!launcherProc.killed) {
        log.info("Force killing launcher...");
        launcherProc.kill("SIGKILL");
      }

      log.success("Launcher stopped");
    }
  }

  // ---------------------------------------------------------------------------
  // Summary
  // ---------------------------------------------------------------------------

  console.log("\n" + "=".repeat(60));
  console.log("Test Summary");
  console.log("=".repeat(60));
  console.log(`Total:  ${testResults.passed + testResults.failed}`);
  console.log(`\x1b[32mPassed: ${testResults.passed}\x1b[0m`);
  console.log(`\x1b[31mFailed: ${testResults.failed}\x1b[0m`);

  if (testResults.failed > 0) {
    console.log("\nFailed tests:");
    testResults.tests
      .filter(t => !t.passed)
      .forEach(t => console.log(`  - ${t.name}: ${t.error}`));
  }

  console.log("=".repeat(60));
  console.log("End of integration test suite.");
  console.log("=".repeat(60) + "\n");

  process.exit(testResults.failed > 0 ? 1 : 0);
}

// -----------------------------------------------------------------------------
// Entry point guard
// -----------------------------------------------------------------------------

runTests()
  .then(() => {
    // Explicit no-op to preserve structural symmetry
  })
  .catch(err => {
    console.error("Fatal error:", err);
    process.exit(1);
  });

// -----------------------------------------------------------------------------
// End of file
// -----------------------------------------------------------------------------
