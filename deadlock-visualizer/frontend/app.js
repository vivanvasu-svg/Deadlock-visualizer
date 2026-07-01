const speedSlider = document.getElementById('speedSlider');
const speedValText = document.getElementById('speedVal');
const btnRestart = document.getElementById('btnRestart');
const btnPrev = document.getElementById('btnPrev');
const btnPlay = document.getElementById('btnPlay');
const btnNext = document.getElementById('btnNext');
const currentStepText = document.getElementById('currentStepText');
const totalStepsText = document.getElementById('totalStepsText');
const scenarioTitle = document.getElementById('currentScenarioName');
const scenarioDesc = document.getElementById('scenarioDesc');
const scenarioAlgoTag = document.getElementById('scenarioAlgo');
const graphContainer = document.getElementById('graphContainer');
const matrixContainer = document.getElementById('matrixContainer');
const canvas = document.getElementById('graphCanvas');
const ctx = canvas.getContext('2d');
const deadlockBanner = document.getElementById('deadlockBanner');
const deadlockCyclePathText = document.getElementById('deadlockCyclePath');
const bankersTableBody = document.getElementById('bankersTableBody');
const availContainer = document.getElementById('availVectorContainer');
const safetyBanner = document.getElementById('safetySequenceBanner');
const safetyText = document.getElementById('safetySequenceText');
const consoleLogArea = document.getElementById('consoleLogArea');
const btnClearLog = document.getElementById('btnClearLog');

const API_URL = '/api/simulate';
let currentScenario = 'rag_cycle';
let simulationData = null;
let currentStep = 0;
let isPlaying = false;
let playbackInterval = null;
let speed = 1000;

let nodePositions = { processes: {}, resources: {} };
let animatedParticles = [];

const SCENARIOS = {
    rag_cycle: {
        name: 'Resource Allocation Graph Cycle',
        type: 'Wait-for-Graph',
        desc: 'Three processes (P0, P1, P2) and three single-unit resources (R0, R1, R2). Demonstration of mutual wait conditions resulting in a classical system deadlock.'
    },
    dining_philosophers: {
        name: 'Dining Philosophers Deadlock',
        type: 'Wait-for-Graph',
        desc: 'Five philosophers (P0-P4) sitting around a table with five chopsticks (F0-F4). Each picks up a left chopstick, creating a circular dependency when requesting the right chopstick.'
    },
    bankers_safe: {
        name: "Banker's Safety Execution (Safe)",
        type: "Banker's Algorithm",
        desc: "Five processes requesting multiple instance resources (A, B, C). Evaluates system safety at each request and schedules a valid safe sequence to termination."
    },
    bankers_unsafe: {
        name: "Banker's State Prevention (Unsafe)",
        type: "Banker's Algorithm",
        desc: "A process makes a request which would exhaust safety buffers. The Banker checks the request, detects that no safe sequence exists, and denies/defers the request to prevent deadlock."
    }
};

init();

function init() {
    document.querySelectorAll('.btn-scenario').forEach(button => {
        button.addEventListener('click', (e) => {
            document.querySelectorAll('.btn-scenario').forEach(b => b.classList.remove('active'));
            const targetBtn = e.target.closest('.btn-scenario');
            targetBtn.classList.add('active');
            switchScenario(targetBtn.dataset.scenario);
        });
    });

    speedSlider.addEventListener('input', (e) => {
        speed = parseInt(e.target.value);
        speedValText.textContent = `${(speed / 1000).toFixed(2)}s / step`;
        if (isPlaying) {
            pauseSimulation();
            playSimulation();
        }
    });

    btnRestart.addEventListener('click', restartSimulation);
    btnPrev.addEventListener('click', stepPrev);
    btnPlay.addEventListener('click', togglePlay);
    btnNext.addEventListener('click', stepNext);
    btnClearLog.addEventListener('click', () => {
        consoleLogArea.innerHTML = '<div class="console-line system-line">[Logs cleared. Proceed stepping to capture trace.]</div>';
    });

    window.addEventListener('resize', handleCanvasResize);

    switchScenario('rag_cycle');
    requestAnimationFrame(animateLoop);
}

function handleCanvasResize() {
    if (currentScenario === 'rag_cycle' || currentScenario === 'dining_philosophers') {
        const rect = canvas.parentNode.getBoundingClientRect();
        canvas.width = rect.width * window.devicePixelRatio;
        canvas.height = rect.height * window.devicePixelRatio;
        canvas.style.width = `${rect.width}px`;
        canvas.style.height = `${rect.height}px`;
        ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        calculateNodePositions();
    }
}

async function switchScenario(scenario) {
    pauseSimulation();
    currentScenario = scenario;

    const info = SCENARIOS[scenario];
    scenarioTitle.textContent = info.name;
    scenarioDesc.textContent = info.desc;
    scenarioAlgoTag.textContent = `Type: ${info.type}`;

    if (info.type === "Banker's Algorithm") {
        graphContainer.classList.add('hidden');
        matrixContainer.classList.remove('hidden');
    } else {
        graphContainer.classList.remove('hidden');
        matrixContainer.classList.add('hidden');
        setTimeout(handleCanvasResize, 50);
    }

    consoleLogArea.innerHTML = '<div class="console-line system-line">[Contacting compilation engine...]</div>';

    try {
        const res = await fetch(`${API_URL}?scenario=${scenario}`);
        if (!res.ok) throw new Error(`HTTP Error ${res.status}`);

        simulationData = await res.json();
        currentStep = 0;

        totalStepsText.textContent = simulationData.timeline.length - 1;
        updateStepUI();

        consoleLogArea.innerHTML = `<div class="console-line system-line">[Simulation trace loaded: ${simulationData.timeline.length} steps]</div>`;
        logMessageToConsole(simulationData.timeline[0]);
    } catch (err) {
        console.error(err);
        consoleLogArea.innerHTML = `<div class="console-line system-line" style="color:var(--color-danger)">Error loading simulation: ${err.message}. Check that C backend server is online and running.</div>`;
    }
}

function updateStepUI() {
    if (!simulationData) return;

    currentStepText.textContent = currentStep;
    const stepData = simulationData.timeline[currentStep];

    if (simulationData.type === 'RAG') {
        renderGraphState(stepData);
    } else {
        renderBankersState(stepData);
    }
}

function logMessageToConsole(stepData) {
    if (!stepData) return;

    let typeClass = 'system-line';
    if (stepData.event === 'REQUEST' || stepData.event === 'WAIT') typeClass = 'request-line';
    else if (stepData.event === 'ALLOCATE') typeClass = 'allocate-line';
    else if (stepData.event === 'WAIT') typeClass = 'wait-line';
    else if (stepData.event === 'CHECK_DEADLOCK' || stepData.event === 'BANKERS_CHECK') typeClass = 'check-line';
    else if (stepData.event === 'DEADLOCK_DETECTED' || stepData.event === 'BANKERS_UNSAFE') typeClass = 'deadlock-line';
    else if (stepData.event === 'BANKERS_SAFE' || stepData.event === 'RELEASE') typeClass = 'success-line';

    const timestamp = new Date().toLocaleTimeString();
    const line = document.createElement('div');
    line.className = `console-line ${typeClass}`;
    line.innerHTML = `<span class="system-line">[${timestamp}] Step ${stepData.step}:</span> ${stepData.message}`;

    consoleLogArea.appendChild(line);
    consoleLogArea.scrollTop = consoleLogArea.scrollHeight;
}

function calculateNodePositions() {
    if (!simulationData || simulationData.type !== 'RAG') return;

    const w = canvas.width / window.devicePixelRatio;
    const h = canvas.height / window.devicePixelRatio;
    const cx = w / 2;
    const cy = h / 2;

    nodePositions = { processes: {}, resources: {} };

    const processes = simulationData.init.processes;
    const resources = simulationData.init.resources;

    const np = processes.length;
    const nr = resources.length;

    const rOuter = Math.min(w, h) * 0.35;
    const rInner = Math.min(w, h) * 0.18;

    processes.forEach((p, idx) => {
        const theta = (2 * Math.PI * idx) / np - Math.PI / 2;
        nodePositions.processes[p.id] = {
            id: p.id,
            name: p.name,
            x: cx + rOuter * Math.cos(theta),
            y: cy + rOuter * Math.sin(theta)
        };
    });

    resources.forEach((r, idx) => {
        const theta = (2 * Math.PI * idx) / nr - Math.PI / 2 + (Math.PI / nr);
        nodePositions.resources[r.id] = {
            id: r.id,
            name: r.name,
            x: cx + rInner * Math.cos(theta),
            y: cy + rInner * Math.sin(theta)
        };
    });
}

function renderGraphState(stepData) {
    if (!stepData) return;

    if (stepData.event === 'DEADLOCK_DETECTED' && stepData.cycle) {
        deadlockBanner.classList.remove('hidden');

        const pathNames = stepData.cycle.map(nodeIdx => {
            const numP = simulationData.init.num_processes;
            if (nodeIdx < numP) {
                return simulationData.init.processes[nodeIdx].name;
            } else {
                return simulationData.init.resources[nodeIdx - numP].name;
            }
        });

        deadlockCyclePathText.textContent = `Path: ${pathNames.join(' \u2192 ')}`;
    } else {
        deadlockBanner.classList.add('hidden');
    }
}

function animateLoop() {
    requestAnimationFrame(animateLoop);

    if (!simulationData || simulationData.type !== 'RAG') return;

    const w = canvas.width / window.devicePixelRatio;
    const h = canvas.height / window.devicePixelRatio;

    ctx.clearRect(0, 0, w, h);

    const stepData = simulationData.timeline[currentStep];
    if (!stepData || !stepData.state) return;

    const { requests, allocation } = stepData.state;

    let isDeadlock = (stepData.event === 'DEADLOCK_DETECTED');
    let cycleNodes = stepData.cycle || [];

    function isEdgeInCycle(u, v) {
        if (!isDeadlock) return false;

        for (let i = 0; i < cycleNodes.length - 1; i++) {
            if (cycleNodes[i] === u && cycleNodes[i + 1] === v) return true;
        }
        return false;
    }

    const numP = simulationData.init.num_processes;

    for (let p = 0; p < requests.length; p++) {
        for (let r = 0; r < requests[p].length; r++) {
            if (requests[p][r] > 0) {
                const pPos = nodePositions.processes[p];
                const rPos = nodePositions.resources[r];
                if (!pPos || !rPos) continue;

                const u = p;
                const v = numP + r;
                const inCycle = isEdgeInCycle(u, v);

                drawDirectedEdge(pPos.x, pPos.y, rPos.x, rPos.y, 25, 20, {
                    color: inCycle ? 'var(--color-danger)' : 'var(--color-warn)',
                    dashed: true,
                    lineWidth: inCycle ? 3 : 1.5,
                    pulse: inCycle
                });
            }
        }
    }

    for (let r = 0; r < allocation.length; r++) {
        for (let p = 0; p < allocation[r].length; p++) {
            if (allocation[r][p] > 0) {
                const pPos = nodePositions.processes[p];
                const rPos = nodePositions.resources[r];
                if (!pPos || !rPos) continue;

                const u = numP + r;
                const v = p;
                const inCycle = isEdgeInCycle(u, v);

                drawDirectedEdge(rPos.x, rPos.y, pPos.x, pPos.y, 20, 25, {
                    color: inCycle ? 'var(--color-danger)' : 'var(--color-process)',
                    dashed: false,
                    lineWidth: inCycle ? 3 : 2,
                    pulse: inCycle
                });
            }
        }
    }

    for (const rId in nodePositions.resources) {
        const r = nodePositions.resources[rId];
        const rIntId = parseInt(rId);
        const inCycle = isDeadlock && cycleNodes.includes(numP + rIntId);
        const size = 34;

        ctx.save();
        ctx.shadowBlur = inCycle ? 20 : 6;
        ctx.shadowColor = inCycle ? 'var(--color-danger)' : 'var(--color-resource)';
        ctx.fillStyle = 'rgba(17, 24, 39, 0.95)';
        ctx.strokeStyle = inCycle ? 'var(--color-danger)' : 'var(--color-resource)';
        ctx.lineWidth = inCycle ? 3 : 2;

        ctx.beginPath();
        ctx.rect(r.x - size / 2, r.y - size / 2, size, size);
        ctx.fill();
        ctx.stroke();
        ctx.restore();

        ctx.fillStyle = inCycle ? 'var(--color-danger)' : 'rgba(59, 130, 246, 0.4)';
        ctx.beginPath();
        ctx.arc(r.x, r.y, 3, 0, Math.PI * 2);
        ctx.fill();

        ctx.fillStyle = 'var(--text-primary)';
        ctx.font = '700 11px var(--font-heading)';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(r.name, r.x, r.y - 28);
    }

    for (const pId in nodePositions.processes) {
        const p = nodePositions.processes[pId];
        const pIntId = parseInt(pId);
        const inCycle = isDeadlock && cycleNodes.includes(pIntId);
        const radius = 20;

        ctx.save();
        ctx.shadowBlur = inCycle ? 20 : 6;
        ctx.shadowColor = inCycle ? 'var(--color-danger)' : 'var(--color-process)';
        ctx.fillStyle = 'rgba(17, 24, 39, 0.95)';
        ctx.strokeStyle = inCycle ? 'var(--color-danger)' : 'var(--color-process)';
        ctx.lineWidth = inCycle ? 3 : 2;

        ctx.beginPath();
        ctx.arc(p.x, p.y, radius, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
        ctx.restore();

        ctx.fillStyle = 'var(--text-primary)';
        ctx.font = '700 12px var(--font-heading)';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(p.name, p.x, p.y);
    }
}

function drawDirectedEdge(x1, y1, x2, y2, offsetStart, offsetEnd, options = {}) {
    const color = options.color || '#fff';
    const dashed = options.dashed || false;
    const lineWidth = options.lineWidth || 1.5;
    const pulse = options.pulse || false;

    const dx = x2 - x1;
    const dy = y2 - y1;
    const angle = Math.atan2(dy, dx);

    const sx = x1 + offsetStart * Math.cos(angle);
    const sy = y1 + offsetStart * Math.sin(angle);
    const ex = x2 - offsetEnd * Math.cos(angle);
    const ey = y2 - offsetEnd * Math.sin(angle);

    ctx.save();
    ctx.strokeStyle = color;
    ctx.fillStyle = color;
    ctx.lineWidth = lineWidth;

    if (dashed) {
        ctx.setLineDash([6, 4]);
        if (pulse) {
            ctx.lineDashOffset = -(Date.now() / 60) % 10;
        }
    } else if (pulse) {
        ctx.shadowBlur = 10;
        ctx.shadowColor = 'var(--color-danger)';
    }

    ctx.beginPath();
    ctx.moveTo(sx, sy);
    ctx.lineTo(ex, ey);
    ctx.stroke();

    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(ex, ey);

    const arrowWidth = 7;
    const arrowLength = 9;
    ctx.lineTo(
        ex - arrowLength * Math.cos(angle) + arrowWidth * Math.sin(angle),
        ey - arrowLength * Math.sin(angle) - arrowWidth * Math.cos(angle)
    );
    ctx.lineTo(
        ex - arrowLength * Math.cos(angle) - arrowWidth * Math.sin(angle),
        ey - arrowLength * Math.sin(angle) + arrowWidth * Math.cos(angle)
    );
    ctx.closePath();
    ctx.fill();
    ctx.restore();
}

function renderBankersState(stepData) {
    if (!stepData || !stepData.state) return;

    const { available, allocation, max, need } = stepData.state;
    const processes = simulationData.init.processes;
    const numRes = simulationData.init.num_resources;

    availContainer.innerHTML = '';
    for (let r = 0; r < numRes; r++) {
        const val = available[r];
        const letter = String.fromCharCode(65 + r);
        availContainer.innerHTML += `
            <div class="avail-box">
                <div class="val">${val}</div>
                <div class="lbl">Res ${letter}</div>
            </div>
        `;
    }

    bankersTableBody.innerHTML = '';
    const actingProcess = stepData.process_id;

    processes.forEach((proc, pIdx) => {
        const isTarget = (proc.id === actingProcess);

        let rowClass = '';
        let statusBadge = '<span class="status-badge queued">Queued</span>';

        let totalAllocated = 0;
        for (let r = 0; r < numRes; r++) {
            totalAllocated += allocation[pIdx][r];
        }

        if (totalAllocated === 0 && stepData.step > 0) {
            statusBadge = '<span class="status-badge finished">Finished</span>';
            rowClass = 'row-completed';
        } else if (isTarget) {
            if (stepData.event === 'REQUEST' || stepData.event === 'WAIT') {
                statusBadge = '<span class="status-badge waiting">Requesting</span>';
                rowClass = 'row-waiting';
            } else if (stepData.event === 'BANKERS_UNSAFE') {
                statusBadge = '<span class="status-badge waiting">Rejected</span>';
                rowClass = 'row-waiting';
            } else {
                statusBadge = '<span class="status-badge finished">Active</span>';
                rowClass = 'row-active';
            }
        }

        let allocCells = '';
        let maxCells = '';
        let needCells = '';

        for (let r = 0; r < numRes; r++) {
            allocCells += `<td class="highlight-alloc">${allocation[pIdx][r]}</td>`;
            maxCells += `<td>${max[pIdx][r]}</td>`;
            needCells += `<td class="highlight-need">${need[pIdx][r]}</td>`;
        }

        bankersTableBody.innerHTML += `
            <tr class="${rowClass}">
                <td style="font-weight:700; color:var(--text-primary)">${proc.name}</td>
                ${allocCells}
                ${maxCells}
                ${needCells}
                <td class="col-status">${statusBadge}</td>
            </tr>
        `;
    });

    if (stepData.event === 'BANKERS_CHECK' || stepData.event === 'BANKERS_SAFE' || stepData.event === 'BANKERS_UNSAFE') {
        const safe = stepData.is_safe;
        safetyBanner.className = `safety-banner ${safe ? 'state-safe' : 'state-unsafe'}`;

        if (safe && stepData.safe_sequence) {
            const seqStr = stepData.safe_sequence.map(p => `P${p}`).join(' \u2192 ');
            safetyText.textContent = `System is SAFE. Safe Sequence found: ${seqStr}`;
        } else {
            safetyText.textContent = `System is UNSAFE! Cannot build safe scheduling sequence. Request blocked.`;
        }
        safetyBanner.classList.remove('hidden');
    } else {
        safetyBanner.classList.add('hidden');
    }
}

function playSimulation() {
    if (isPlaying) return;

    isPlaying = true;
    btnPlay.innerHTML = '&#10074;&#10074;';
    btnPlay.classList.add('playing');

    playbackInterval = setInterval(() => {
        stepNext();
        if (currentStep >= simulationData.timeline.length - 1) {
            pauseSimulation();
        }
    }, speed);
}

function pauseSimulation() {
    if (!isPlaying) return;

    isPlaying = false;
    btnPlay.innerHTML = '&#9658;';
    btnPlay.classList.remove('playing');
    clearInterval(playbackInterval);
}

function togglePlay() {
    if (isPlaying) {
        pauseSimulation();
    } else {
        if (currentStep >= simulationData.timeline.length - 1) {
            restartSimulation();
        }
        playSimulation();
    }
}

function restartSimulation() {
    pauseSimulation();
    currentStep = 0;
    consoleLogArea.innerHTML = '<div class="console-line system-line">[Restarting Simulation...]</div>';
    logMessageToConsole(simulationData.timeline[0]);
    updateStepUI();
}

function stepNext() {
    if (!simulationData) return;

    if (currentStep < simulationData.timeline.length - 1) {
        currentStep++;
        updateStepUI();
        logMessageToConsole(simulationData.timeline[currentStep]);
    }
}

function stepPrev() {
    if (!simulationData) return;

    if (currentStep > 0) {
        currentStep--;
        const consoleLines = consoleLogArea.querySelectorAll('.console-line');
        if (consoleLines.length > 1) {
            consoleLogArea.removeChild(consoleLines[consoleLines.length - 1]);
        }
        updateStepUI();
    }
}
