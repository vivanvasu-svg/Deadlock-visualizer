const http = require('http');
const fs = require('fs');
const path = require('path');
const { execFile, exec } = require('child_process');

const PORT = 3000;
const BIN_PATH = path.join(__dirname, 'bin', 'deadlock');
const FRONTEND_DIR = path.join(__dirname, 'frontend');

function checkAndBuildBinary(callback) {
    if (fs.existsSync(BIN_PATH)) {
        return callback(null);
    }
    exec('make', { cwd: __dirname }, (error, stdout, stderr) => {
        if (error) {
            return callback(error);
        }
        callback(null);
    });
}

const MIME_TYPES = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'application/javascript',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon'
};

const server = http.createServer((req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    const parsedUrl = new URL(req.url, `http://${req.headers.host}`);
    const pathname = parsedUrl.pathname;

    if (pathname === '/api/simulate') {
        const scenario = parsedUrl.searchParams.get('scenario') || 'rag_cycle';
        const allowedScenarios = ['rag_cycle', 'dining_philosophers', 'bankers_safe', 'bankers_unsafe'];

        if (!allowedScenarios.includes(scenario)) {
            res.writeHead(400, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: `Invalid scenario. Must be one of: ${allowedScenarios.join(', ')}` }));
            return;
        }

        checkAndBuildBinary((err) => {
            if (err) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Failed to compile C binary', details: err.message }));
                return;
            }

            execFile(BIN_PATH, ['--scenario', scenario], (execErr, stdout, execStderr) => {
                if (execErr) {
                    res.writeHead(500, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'Simulator process error', details: execStderr }));
                    return;
                }

                try {
                    const jsonResult = JSON.parse(stdout);
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify(jsonResult));
                } catch (parseErr) {
                    res.writeHead(500, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({
                        error: 'Failed to parse simulator output',
                        raw_output: stdout,
                        parse_error: parseErr.message
                    }));
                }
            });
        });
        return;
    }

    let filePath = path.join(FRONTEND_DIR, pathname === '/' ? 'index.html' : pathname);

    if (!filePath.startsWith(FRONTEND_DIR)) {
        res.writeHead(403);
        res.end('Forbidden');
        return;
    }

    fs.stat(filePath, (err, stats) => {
        if (err || !stats.isFile()) {
            const fallbackPath = path.join(FRONTEND_DIR, 'index.html');
            fs.readFile(fallbackPath, (fallbackErr, data) => {
                if (fallbackErr) {
                    res.writeHead(404, { 'Content-Type': 'text/plain' });
                    res.end('404 Not Found');
                } else {
                    res.writeHead(200, { 'Content-Type': 'text/html' });
                    res.end(data);
                }
            });
            return;
        }

        const ext = path.extname(filePath).toLowerCase();
        const contentType = MIME_TYPES[ext] || 'application/octet-stream';

        fs.readFile(filePath, (readErr, data) => {
            if (readErr) {
                res.writeHead(500);
                res.end('Internal Server Error');
            } else {
                res.writeHead(200, { 'Content-Type': contentType });
                res.end(data);
            }
        });
    });
});

checkAndBuildBinary((err) => {
    if (err) {
        console.error(err);
    }
    server.listen(PORT, () => {
        console.log(`===============================================`);
        console.log(` Server Status: ACTIVE                         `);
        console.log(` URL: http://localhost:${PORT}                  `);
        console.log(`===============================================`);
    });
});
