// tests/integration.test.js
// Integration tests for APM system
// Run with: node tests/integration.test.js

const http = require("http");
const { spawn } = require("child_process");
const path = require("path");

const TEST_CONFIG = {
  BACKEND_PORT: 8888,  // Use different ports for testing
  UI_PORT: 5555,
  TIMEOUT: 30000
};

let testResults = {
  passed: 0,
  failed: 0,
  tests: []
};

// Logging utilities
const log = {
  info: (msg) => console.log(`[INFO] ${msg}`),
  success: (msg) => console.log(`\x1b[32m✓\x1b[0m ${msg}`),
  fail: (msg) => console.log(`\x1b[31m✗\x1b[0m ${msg}`),
  section: (msg) => console.log(`\n\x1b[36m${msg}\x1b[0m`)
};

// HTTP request helper
function httpRequest(options, postData = null) {
  return new Promise((resolve, reject) => {
    const req = http.request(options, (res) => {
      let data = "";
      res.on("data", chunk => data += chunk);
      res.on("end", () => resolve({ statusCode: res.statusCode, data, headers: res.headers }));
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

// Test runner
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

// Wait for service to be ready
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

// Start launcher for testing
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

    // Timeout
    setTimeout(() => {
      if (proc.exitCode === null) {
        reject(new Error("Launcher startup timeout\n" + startupLogs));
      }
    }, TEST_CONFIG.TIMEOUT);
  });
}

// Test suite
async function runTests() {
  console.log("\n" + "=".repeat(60));
  console.log("APM Integration Test Suite");
  console.log("=".repeat(60));

  let launcherProc = null;

  try {
    // Start the system
    log.section("Starting APM System...");
    const result = await startLauncher();
    launcherProc = result.proc;
    log.success("System started successfully");

    // Wait for services
    log.info("Waiting for services to be ready...");
    await waitForService(TEST_CONFIG.BACKEND_PORT, "/health");
    await waitForService(TEST_CONFIG.UI_PORT, "/");
    log.success("All services ready");

    // Backend tests
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

    await test("Backend health check response time < 100ms", async () => {
      const start = Date.now();
      await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.BACKEND_PORT,
        path: "/health",
        method: "GET"
      });
      const duration = Date.now() - start;
      if (duration > 100) {
        throw new Error(`Response took ${duration}ms (expected < 100ms)`);
      }
    });

    await test("Backend handles 10 concurrent requests", async () => {
      const requests = Array(10).fill().map(() =>
        httpRequest({
          hostname: "localhost",
          port: TEST_CONFIG.BACKEND_PORT,
          path: "/health",
          method: "GET"
        })
      );
      const results = await Promise.all(requests);
      const allOk = results.every(r => r.statusCode === 200);
      if (!allOk) {
        throw new Error("Some requests failed");
      }
    });

    await test("Backend returns proper CORS headers (if applicable)", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.BACKEND_PORT,
        path: "/health",
        method: "GET"
      });
      // Check if CORS headers exist (optional, depends on backend impl)
      // This test can be customized based on your backend's actual behavior
    });

    // UI tests
    log.section("UI Server Tests");

    await test("UI server returns 200 for /", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.UI_PORT,
        path: "/",
        method: "GET"
      });
      if (res.statusCode !== 200) {
        throw new Error(`Expected 200, got ${res.statusCode}`);
      }
    });

    await test("UI server returns HTML content", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.UI_PORT,
        path: "/",
        method: "GET"
      });
      const isHtml = res.data.toLowerCase().includes("<html") || 
                     res.data.toLowerCase().includes("<!doctype");
      if (!isHtml) {
        throw new Error("Response does not contain HTML");
      }
    });

    await test("UI server has security headers", async () => {
      const res = await httpRequest({
        hostname: "localhost",
        port: TEST_CONFIG.UI_PORT,
        path: "/",
        method: "GET"
      });
      const requiredHeaders = [
        "x-content-type-options",
        "x-frame-options",
        "x-xss-protection"
      ];
      const missingHeaders = requiredHeaders.filter(h => !res.headers[h]);
      if (missingHeaders.length > 0) {
        throw new Error(`Missing headers: ${missingHeaders.join(", ")}`);
      }
    });

    await test("UI server returns 404 for invalid paths", async () => {
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

    // Load tests
    log.section("Load Tests");

    await test("Backend handles 100 requests in sequence", async () => {
      const start = Date.now();
      for (let i = 0; i < 100; i++) {
        await httpRequest({
          hostname: "localhost",
          port: TEST_CONFIG.BACKEND_PORT,
          path: "/health",
          method: "GET"
        });
      }
      const duration = Date.now() - start;
      log.info(`  Completed 100 requests in ${duration}ms (${(duration/100).toFixed(2)}ms avg)`);
    });

    await test("Backend handles 50 concurrent requests", async () => {
      const start = Date.now();
      const requests = Array(50).fill().map(() =>
        httpRequest({
          hostname: "localhost",
          port: TEST_CONFIG.BACKEND_PORT,
          path: "/health",
          method: "GET"
        })
      );
      await Promise.all(requests);
      const duration = Date.now() - start;
      log.info(`  Completed 50 concurrent requests in ${duration}ms`);
    });

  } catch (err) {
    log.fail(`Test suite error: ${err.message}`);
    if (err.stack) console.error(err.stack);
  } finally {
    // Cleanup
    log.section("Cleanup");
    if (launcherProc && !launcherProc.killed) {
      log.info("Stopping launcher...");
      launcherProc.kill("SIGTERM");
      
      // Wait for graceful shutdown
      await new Promise(resolve => {
        setTimeout(() => {
          if (!launcherProc.killed) {
            log.info("Force killing launcher...");
            launcherProc.kill("SIGKILL");
          }
          resolve();
        }, 5000);
      });
      
      log.success("Launcher stopped");
    }
  }

  // Print summary
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
  
  console.log("=".repeat(60) + "\n");

  // Exit with appropriate code
  process.exit(testResults.failed > 0 ? 1 : 0);
}

// Run tests
runTests().catch(err => {
  console.error("Fatal error:", err);
  process.exit(1);
});
