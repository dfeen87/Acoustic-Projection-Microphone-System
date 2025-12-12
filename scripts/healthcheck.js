#!/usr/bin/env node
// scripts/healthcheck.js
// Production health check and validation script

const http = require("http");
const fs = require("fs");
const path = require("path");
const { execSync } = require("child_process");

const COLORS = {
  reset: "\x1b[0m",
  red: "\x1b[31m",
  green: "\x1b[32m",
  yellow: "\x1b[33m",
  blue: "\x1b[34m",
  cyan: "\x1b[36m"
};

function log(msg, color = "reset") {
  console.log(`${COLORS[color]}${msg}${COLORS.reset}`);
}

function checkSync(name, fn) {
  try {
    process.stdout.write(`  ${name}... `);
    fn();
    log("✅ PASS", "green");
    return true;
  } catch (err) {
    log(`❌ FAIL: ${err.message}`, "red");
    return false;
  }
}

async function checkAsync(name, fn) {
  try {
    process.stdout.write(`  ${name}... `);
    await fn();
    log("✅ PASS", "green");
    return true;
  } catch (err) {
    log(`❌ FAIL: ${err.message}`, "red");
    return false;
  }
}

function httpGet(url, timeout = 5000) {
  return new Promise((resolve, reject) => {
    const req = http.get(url, { timeout }, (res) => {
      let data = "";
      res.on("data", chunk => data += chunk);
      res.on("end", () => resolve({ statusCode: res.statusCode, data }));
    });
    req.on("error", reject);
    req.on("timeout", () => {
      req.destroy();
      reject(new Error("Request timeout"));
    });
  });
}

async function main() {
  log("\n" + "=".repeat(60), "cyan");
  log("APM System Health Check", "cyan");
  log("=".repeat(60) + "\n", "cyan");

  const results = {
    passed: 0,
    failed: 0
  };

  // 1. Environment checks
  log("1. Environment Prerequisites", "blue");
  
  if (checkSync("Node.js version", () => {
    const version = process.version;
    const major = parseInt(version.slice(1).split(".")[0]);
    if (major < 14) throw new Error(`Node ${version} too old, need 14+`);
  })) results.passed++; else results.failed++;

  if (checkSync("CMake installed", () => {
    try {
      execSync("cmake --version", { stdio: "pipe" });
    } catch {
      throw new Error("CMake not found");
    }
  })) results.passed++; else results.failed++;

  if (checkSync("C++ compiler available", () => {
    let found = false;
    const compilers = ["g++", "clang++", "cl"];
    for (const compiler of compilers) {
      try {
        execSync(`${compiler} --version`, { stdio: "pipe" });
        found = true;
        break;
      } catch {}
    }
    if (!found) throw new Error("No C++ compiler found (g++/clang++/MSVC)");
  })) results.passed++; else results.failed++;

  // 2. File structure checks
  log("\n2. File Structure", "blue");
  
  const rootDir = path.join(__dirname, "..");
  
  if (checkSync("launcher/apm_launcher.js exists", () => {
    const file = path.join(rootDir, "launcher", "apm_launcher.js");
    if (!fs.existsSync(file)) throw new Error("File not found");
  })) results.passed++; else results.failed++;

  if (checkSync("launcher/package.json exists", () => {
    const file = path.join(rootDir, "launcher", "package.json");
    if (!fs.existsSync(file)) throw new Error("File not found");
  })) results.passed++; else results.failed++;

  if (checkSync("apm-dashboard.html exists", () => {
    const file = path.join(rootDir, "apm-dashboard.html");
    if (!fs.existsSync(file)) throw new Error("File not found");
  })) results.passed++; else results.failed++;

  if (checkSync("main.cpp exists", () => {
    const file = path.join(rootDir, "main.cpp");
    if (!fs.existsSync(file)) throw new Error("File not found");
  })) results.passed++; else results.failed++;

  if (checkSync("CMakeLists.txt exists", () => {
    const file = path.join(rootDir, "CMakeLists.txt");
    if (!fs.existsSync(file)) throw new Error("File not found");
  })) results.passed++; else results.failed++;

  // 3. Build checks
  log("\n3. Build System", "blue");
  
  if (checkSync("Backend binary exists", () => {
    const binaries = [
      path.join(rootDir, "apm_backend"),
      path.join(rootDir, "apm_backend.exe"),
      path.join(rootDir, "build", "apm_backend"),
      path.join(rootDir, "build", "apm_backend.exe"),
      path.join(rootDir, "build", "Release", "apm_backend.exe"),
      path.join(rootDir, "build", "Debug", "apm_backend.exe")
    ];
    
    const exists = binaries.some(b => fs.existsSync(b));
    if (!exists) {
      throw new Error(
        "Backend not built. Run: cmake -B build && cmake --build build"
      );
    }
  })) results.passed++; else results.failed++;

  // 4. Dependencies check
  log("\n4. Dependencies", "blue");
  
  if (checkSync("launcher dependencies installed", () => {
    const nodeModules = path.join(rootDir, "launcher", "node_modules");
    if (!fs.existsSync(nodeModules)) {
      throw new Error("Run: cd launcher && npm install");
    }
  })) results.passed++; else results.failed++;

  // 5. Runtime checks (if system is running)
  log("\n5. Runtime Status", "blue");
  
  const BACKEND_PORT = process.env.APM_BACKEND_PORT || 8080;
  const UI_PORT = process.env.APM_UI_PORT || 4173;

  const backendRunning = await checkAsync("Backend /health endpoint", async () => {
    const res = await httpGet(`http://localhost:${BACKEND_PORT}/health`, 2000);
    if (res.statusCode !== 200) {
      throw new Error(`HTTP ${res.statusCode}`);
    }
  });
  if (backendRunning) results.passed++; else results.failed++;

  const uiRunning = await checkAsync("UI server responding", async () => {
    const res = await httpGet(`http://localhost:${UI_PORT}/`, 2000);
    if (res.statusCode !== 200) {
      throw new Error(`HTTP ${res.statusCode}`);
    }
    if (!res.data.includes("html") && !res.data.includes("HTML")) {
      throw new Error("Response doesn't look like HTML");
    }
  });
  if (uiRunning) results.passed++; else results.failed++;

  // 6. Port availability (if not running)
  if (!backendRunning || !uiRunning) {
    log("\n6. Port Availability", "blue");
    
    if (!backendRunning) {
      await checkAsync(`Backend port ${BACKEND_PORT} available`, async () => {
        try {
          await httpGet(`http://localhost:${BACKEND_PORT}/health`, 500);
          throw new Error(`Port in use by another service`);
        } catch (err) {
          if (err.message.includes("ECONNREFUSED")) {
            return; // Port is free
          }
          throw err;
        }
      });
    }

    if (!uiRunning) {
      await checkAsync(`UI port ${UI_PORT} available`, async () => {
        try {
          await httpGet(`http://localhost:${UI_PORT}/`, 500);
          throw new Error(`Port in use by another service`);
        } catch (err) {
          if (err.message.includes("ECONNREFUSED")) {
            return; // Port is free
          }
          throw err;
        }
      });
    }
  }

  // Summary
  log("\n" + "=".repeat(60), "cyan");
  log("Summary", "cyan");
  log("=".repeat(60), "cyan");
  
  const total = results.passed + results.failed;
  log(`Total checks: ${total}`, "cyan");
  log(`Passed: ${results.passed}`, results.passed === total ? "green" : "yellow");
  log(`Failed: ${results.failed}`, results.failed === 0 ? "green" : "red");

  if (results.failed === 0) {
    log("\n✅ All checks passed! System is ready.", "green");
    if (!backendRunning) {
      log("\n💡 Start the system with: cd launcher && npm start", "cyan");
    } else {
      log("\n✨ System is currently running!", "green");
    }
    process.exit(0);
  } else {
    log(`\n❌ ${results.failed} check(s) failed. Fix issues above.`, "red");
    process.exit(1);
  }
}

// Run
main().catch(err => {
  log(`\n💥 Fatal error: ${err.message}`, "red");
  if (err.stack) console.error(err.stack);
  process.exit(1);
});
