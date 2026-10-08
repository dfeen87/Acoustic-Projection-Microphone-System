// launcher/apm_launcher.js
const { spawn } = require("child_process");
const http = require("http");
const path = require("path");
const fs = require("fs");
const open = require("open");

// Configuration
const CONFIG = {
  BACKEND_PORT: parseInt(process.env.APM_BACKEND_PORT || "8080", 10),
  UI_PORT: parseInt(process.env.APM_UI_PORT || "4173", 10),
  HEALTH_CHECK_TIMEOUT: 60000, // 60 seconds max wait
  HEALTH_CHECK_INTERVAL: 300,   // Check every 300ms
  STARTUP_DELAY: 500,           // Initial delay before health checks
  MAX_RETRIES: 3,
  // Updated to launch Python backend which provides the API
  BACKEND_EXECUTABLE: process.platform === "win32" ? "python" : "python3",
  BACKEND_ARGS: ["backend/main.py", "--host", "127.0.0.1"],
  UI_HTML_PATHS: [path.join(__dirname, "../ui/dist/index.html")]

};

// State management
const state = {
  backendProcess: null,
  uiServer: null,
  isShuttingDown: false,
  startupAttempts: 0
};

// Logging utility
const logger = {
  info: (msg) => console.log(`[${new Date().toISOString()}] [INFO] ${msg}`),
  error: (msg) => console.error(`[${new Date().toISOString()}] [ERROR] ${msg}`),
  warn: (msg) => console.warn(`[${new Date().toISOString()}] [WARN] ${msg}`),
  success: (msg) => console.log(`[${new Date().toISOString()}] [SUCCESS] ${msg}`)
};

/**
 * Validates that a port number is in the valid range 1–65535
 */
function validatePort(port, name) {
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error(
      `Invalid ${name} port ${port}: must be an integer between 1 and 65535`
    );
  }
}

/**
 * Validates required files and directories exist
 */
function validateEnvironment() {
  logger.info("Validating environment...");

  // Validate configured ports
  validatePort(CONFIG.BACKEND_PORT, "backend");
  validatePort(CONFIG.UI_PORT, "UI");

  // Check backend path
  // Since we are running python, we check for the script existence
  const scriptPath = path.join(__dirname, "..", CONFIG.BACKEND_ARGS[0]);
  if (!fs.existsSync(scriptPath)) {
    throw new Error(
      `Backend script not found at: ${scriptPath}\n`
    );
  }

  // Check UI HTML file
  const uiPath = CONFIG.UI_HTML_PATHS.find((candidate) => fs.existsSync(candidate));
  if (!uiPath) {
    throw new Error(
      `Built UI not found. Run npm ci and npm run build in ui/. Checked:\n  - ${CONFIG.UI_HTML_PATHS.join("\n  - ")}`
    );
  }

  CONFIG.UI_HTML_PATH = uiPath;

  logger.success("Environment validation passed");
}

/**
 * Checks if a port is available
 */
function checkPortAvailable(port) {
  return new Promise((resolve) => {
    const server = http.createServer();
    
    server.once("error", (err) => {
      if (err.code === "EADDRINUSE") {
        resolve(false);
      } else {
        resolve(true);
      }
    });

    server.once("listening", () => {
      server.close();
      resolve(true);
    });

    server.listen(port);
  });
}

/**
 * Waits for backend to become healthy
 */
function waitForBackend() {
  return new Promise((resolve, reject) => {
    const startTime = Date.now();
    let attempts = 0;

    const checkHealth = () => {
      // Timeout check
      if (Date.now() - startTime > CONFIG.HEALTH_CHECK_TIMEOUT) {
        return reject(new Error(
          `Backend health check timed out after ${CONFIG.HEALTH_CHECK_TIMEOUT}ms\n` +
          `Attempted ${attempts} health checks`
        ));
      }

      attempts++;
      
      const req = http.get(
        `http://localhost:${CONFIG.BACKEND_PORT}/health`,
        { timeout: 2000 },
        (res) => {
          if (res.statusCode === 200) {
            logger.success(`Backend healthy after ${attempts} checks (${Date.now() - startTime}ms)`);
            return resolve();
          }
          
          logger.warn(`Health check ${attempts}: received status ${res.statusCode}`);
          setTimeout(checkHealth, CONFIG.HEALTH_CHECK_INTERVAL);
        }
      );

      req.on("error", (err) => {
        // Expected during startup - backend not ready yet
        if (err.code === "ECONNREFUSED" || err.code === "ETIMEDOUT") {
          setTimeout(checkHealth, CONFIG.HEALTH_CHECK_INTERVAL);
        } else {
          logger.error(`Unexpected health check error: ${err.message}`);
          setTimeout(checkHealth, CONFIG.HEALTH_CHECK_INTERVAL);
        }
      });

      req.on("timeout", () => {
        req.destroy();
        setTimeout(checkHealth, CONFIG.HEALTH_CHECK_INTERVAL);
      });
    };

    // Initial delay to let backend start
    setTimeout(checkHealth, CONFIG.STARTUP_DELAY);
  });
}

/**
 * Starts the C++ backend process
 */
function startBackend() {
  return new Promise((resolve, reject) => {
    logger.info("Starting C++ backend...");
    
    // We launch python backend/main.py
    // The working directory should be the repo root
    const workingDir = path.resolve(__dirname, "..");
    const command = CONFIG.BACKEND_EXECUTABLE;
    const args = [...CONFIG.BACKEND_ARGS];

    // Add port argument if needed, backend/main.py supports --port
    args.push("--port", CONFIG.BACKEND_PORT.toString());

    logger.info(`Command: ${command} ${args.join(" ")}`);
    logger.info(`Working directory: ${workingDir}`);
    logger.info(`Backend port: ${CONFIG.BACKEND_PORT}`);

    try {
      const proc = spawn(command, args, {
        stdio: ["ignore", "pipe", "pipe"],
        cwd: workingDir,
        env: {
          ...process.env,
          // Python backend reads args, but we can pass env just in case
        }
      });

      // Capture backend output
      proc.stdout.on("data", (data) => {
        const lines = data.toString().trim().split("\n");
        lines.forEach(line => logger.info(`[Backend] ${line}`));
      });

      proc.stderr.on("data", (data) => {
        const lines = data.toString().trim().split("\n");
        lines.forEach(line => logger.error(`[Backend] ${line}`));
      });

      proc.on("error", (err) => {
        logger.error(`Backend process error: ${err.message}`);
        reject(err);
      });

      proc.on("exit", (code, signal) => {
        if (!state.isShuttingDown) {
          logger.error(`Backend exited unexpectedly: code=${code}, signal=${signal}`);
          gracefulShutdown(1);
        }
      });

      state.backendProcess = proc;
      resolve(proc);

    } catch (err) {
      reject(new Error(`Failed to spawn backend: ${err.message}`));
    }
  });
}

/**
 * Starts the UI HTTP server
 */
function startUiServer() {
  return new Promise((resolve, reject) => {
    logger.info("Starting UI server...");

    const server = http.createServer((req, res) => {
      // Security headers
      res.setHeader("X-Content-Type-Options", "nosniff");
      res.setHeader("X-Frame-Options", "DENY");
      res.setHeader("X-XSS-Protection", "1; mode=block");

      // Forward the built dashboard, assets and API to their existing owner.
      // Keep this listener thin: never expose arbitrary filesystem paths.
      const urlPath = req.url.split("?", 1)[0];
      if (!(urlPath === "/" || urlPath === "/index.html" || urlPath === "/health" ||
          urlPath.startsWith("/assets/") || urlPath.startsWith("/api/"))) {
        res.writeHead(404, { "Content-Type": "text/plain" });
        return res.end("Not Found");
      }
      const headers = { ...req.headers };
      // Uvicorn trusts this loopback proxy. Derive provenance from the client
      // socket instead of passing headers supplied by that client as evidence.
      delete headers["forwarded"];
      delete headers["x-real-ip"];
      headers["x-forwarded-for"] = req.socket.remoteAddress;
      headers["x-forwarded-proto"] = "http";
      const upstream = http.request({
        hostname: "127.0.0.1", port: CONFIG.BACKEND_PORT,
        path: req.url, method: req.method, headers,
      }, (backendResponse) => {
        res.writeHead(backendResponse.statusCode, backendResponse.headers);
        backendResponse.pipe(res);
      });
      upstream.setTimeout(5000, () => upstream.destroy(new Error("Backend timeout")));
      upstream.on("error", () => {
        if (!res.headersSent) res.writeHead(502, { "Content-Type": "text/plain" });
        res.end("Backend unavailable");
      });
      req.on("aborted", () => upstream.destroy());
      req.pipe(upstream);

    });

    server.on("error", (err) => {
      if (err.code === "EADDRINUSE") {
        reject(new Error(`UI port ${CONFIG.UI_PORT} is already in use`));
      } else {
        reject(err);
      }
    });

    server.listen(CONFIG.UI_PORT, "127.0.0.1", () => {
      logger.success(`UI server listening on http://localhost:${CONFIG.UI_PORT}`);
      state.uiServer = server;
      resolve(server);
    });
  });
}

/**
 * Opens the UI in the default browser
 */
async function openBrowser() {
  const url = `http://localhost:${CONFIG.UI_PORT}`;
  logger.info(`Opening browser: ${url}`);

  try {
    await open(url);
    logger.success("Browser opened successfully");
  } catch (err) {
    logger.warn(`Could not open browser automatically: ${err.message}`);
    logger.info(`Please open manually: ${url}`);
  }
}

/**
 * Gracefully shuts down all processes
 */
function gracefulShutdown(exitCode = 0) {
  if (state.isShuttingDown) {
    return; // Already shutting down
  }

  state.isShuttingDown = true;
  logger.info("Initiating graceful shutdown...");

  // Close UI server
  if (state.uiServer) {
    state.uiServer.close(() => {
      logger.info("UI server closed");
    });
  }

  // Stop backend process
  if (state.backendProcess && state.backendProcess.exitCode === null && state.backendProcess.signalCode === null) {
    logger.info("Sending SIGTERM to backend...");
    state.backendProcess.kill("SIGTERM");

    // Force kill after 5 seconds
    setTimeout(() => {
      if (state.backendProcess && state.backendProcess.exitCode === null && state.backendProcess.signalCode === null) {
        logger.warn("Backend did not exit gracefully, forcing SIGKILL");
        state.backendProcess.kill("SIGKILL");
      }
    }, 5000);
  }

  // Exit after cleanup
  setTimeout(() => {
    logger.info(`Exiting with code ${exitCode}`);
    process.exit(exitCode);
  }, 6000);
}

/**
 * Main startup sequence
 */
async function main() {
  try {
    logger.info("=".repeat(60));
    logger.info("APM System Launcher - Production Mode");
    logger.info("=".repeat(60));

    // Pre-flight checks
    validateEnvironment();

    // Check port availability
    const backendPortAvailable = await checkPortAvailable(CONFIG.BACKEND_PORT);
    if (!backendPortAvailable) {
      throw new Error(
        `Backend port ${CONFIG.BACKEND_PORT} is already in use.\n` +
        `Another instance may be running. Stop it or set APM_BACKEND_PORT env variable.`
      );
    }

    const uiPortAvailable = await checkPortAvailable(CONFIG.UI_PORT);
    if (!uiPortAvailable) {
      throw new Error(
        `UI port ${CONFIG.UI_PORT} is already in use.\n` +
        `Set APM_UI_PORT env variable to use a different port.`
      );
    }

    // Start backend
    await startBackend();

    // Wait for backend health
    logger.info("Waiting for backend to become healthy...");
    await waitForBackend();

    // Start UI server
    await startUiServer();

    // Open browser
    await openBrowser();

    logger.info("=".repeat(60));
    logger.success("APM System is fully operational! 🚀");
    logger.info("=".repeat(60));
    logger.info(`Backend API: http://localhost:${CONFIG.BACKEND_PORT}`);
    logger.info(`Dashboard UI: http://localhost:${CONFIG.UI_PORT}`);
    logger.info("Press Ctrl+C to shutdown gracefully");
    logger.info("=".repeat(60));

  } catch (err) {
    logger.error("Startup failed:");
    logger.error(err.message);
    
    if (err.stack && process.env.DEBUG) {
      logger.error(err.stack);
    }

    gracefulShutdown(1);
  }
}

// Signal handlers
process.on("SIGINT", () => {
  logger.info("\nReceived SIGINT (Ctrl+C)");
  gracefulShutdown(0);
});

process.on("SIGTERM", () => {
  logger.info("\nReceived SIGTERM");
  gracefulShutdown(0);
});

process.on("uncaughtException", (err) => {
  logger.error(`Uncaught exception: ${err.message}`);
  if (err.stack) logger.error(err.stack);
  gracefulShutdown(1);
});

process.on("unhandledRejection", (reason, promise) => {
  logger.error(`Unhandled rejection at: ${promise}`);
  logger.error(`Reason: ${reason}`);
  gracefulShutdown(1);
});

// Start the application
main();
