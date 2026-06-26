// server.js — Live Panel IVR v1.4 (SQLite)
const express   = require('express');
const http      = require('http');
const WebSocket = require('ws');
const path      = require('path');
const Database  = require('better-sqlite3');

const app    = express();
const server = http.createServer(app);
const wss    = new WebSocket.Server({ server });

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// ── Base de données SQLite ────────────────────────────────────────────────────
// Stockée dans C:\IVR\ (même dossier que les WAVs) pour persister les données
const DB_PATH = process.env.IVR_DATA_PATH
    ? path.join(process.env.IVR_DATA_PATH, 'ivr_history.db')
    : path.join('C:\\IVR', 'ivr_history.db');

let db;
try {
    db = new Database(DB_PATH);
    db.pragma('journal_mode = WAL'); // performance + concurrent reads
    db.exec(`
        CREATE TABLE IF NOT EXISTS call_history (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            callId      TEXT    NOT NULL,
            phone       TEXT    DEFAULT '',
            phoneDisplay TEXT   DEFAULT '',
            label       TEXT    DEFAULT '',
            profile     TEXT    DEFAULT '',
            startTime   TEXT    DEFAULT '',
            archivedAt  TEXT    DEFAULT '',
            finalState  TEXT    DEFAULT 'DONE',
            stepResults TEXT    DEFAULT '{}',
            eventHistory TEXT   DEFAULT '[]'
        );
        CREATE INDEX IF NOT EXISTS idx_archived ON call_history(archivedAt DESC);
    `);
    console.log(`✅ SQLite connecté: ${DB_PATH}`);
} catch (e) {
    console.warn(`⚠ SQLite non disponible (${e.message}), historique désactivé`);
    db = null;
}

// Préparer les requêtes SQL
const stmtInsert = db ? db.prepare(`
    INSERT INTO call_history
        (callId, phone, phoneDisplay, label, profile, startTime, archivedAt, finalState, stepResults, eventHistory)
    VALUES
        (@callId, @phone, @phoneDisplay, @label, @profile, @startTime, @archivedAt, @finalState, @stepResults, @eventHistory)
`) : null;

const stmtGetHistory = db ? db.prepare(`
    SELECT * FROM call_history ORDER BY id DESC LIMIT ?
`) : null;

const stmtClearHistory = db ? db.prepare(`
    DELETE FROM call_history
`) : null;

const stmtSearchHistory = db ? db.prepare(`
    SELECT * FROM call_history
    WHERE phone LIKE ? OR phoneDisplay LIKE ? OR stepResults LIKE ?
    ORDER BY id DESC LIMIT 50
`) : null;

// ── Appels actifs en mémoire ──────────────────────────────────────────────────
const calls = {};

function broadcast(data) {
    const msg = JSON.stringify(data);
    wss.clients.forEach(c => { if (c.readyState === WebSocket.OPEN) c.send(msg); });
}

function parsePhone(remoteInfo) {
    if (!remoteInfo) return '';
    let m = remoteInfo.match(/sip:([0-9+*#]{4,20})@/);
    if (m) return m[1];
    m = remoteInfo.match(/([0-9]{7,20})/);
    return m ? m[1] : remoteInfo;
}

function formatPhone(num) {
    const d = (num || '').replace(/\D/g,'');
    if (d.length === 11 && d[0] === '1') return `+1 (${d.slice(1,4)}) ${d.slice(4,7)}-${d.slice(7)}`;
    if (d.length === 10) return `(${d.slice(0,3)}) ${d.slice(3,6)}-${d.slice(6)}`;
    return num;
}

function getHistory(limit = 100) {
    if (!stmtGetHistory) return [];
    return stmtGetHistory.all(limit).map(row => ({
        ...row,
        stepResults: JSON.parse(row.stepResults || '{}'),
        history: JSON.parse(row.eventHistory || '[]')
    }));
}

function archiveCall(id) {
    if (!calls[id] || !stmtInsert) return;
    const call = calls[id];
    const hasData = Object.keys(call.stepResults || {}).some(k => call.stepResults[k].value);
    if (!hasData && call.state === 'ACTIVE') return;

    try {
        stmtInsert.run({
            callId:       call.callId,
            phone:        call.phone || '',
            phoneDisplay: call.phoneDisplay || '',
            label:        call.label || '',
            profile:      call.profile || '',
            startTime:    call.startTime || '',
            archivedAt:   now(),
            finalState:   call.state || 'DONE',
            stepResults:  JSON.stringify(call.stepResults || {}),
            eventHistory: JSON.stringify((call.history || []).slice(-20))
        });
        const history = getHistory(100);
        broadcast({ type: 'history_updated', history });
        console.log(`[DB] Appel archivé: ${call.phoneDisplay || call.callId}`);
    } catch (e) {
        console.error('[DB] Erreur archivage:', e.message);
    }
}

// ── WebSocket ─────────────────────────────────────────────────────────────────
wss.on('connection', ws => {
    ws.send(JSON.stringify({
        type: 'init',
        calls,
        history: getHistory(100)
    }));
});

// ── Événements IVR ────────────────────────────────────────────────────────────
app.post('/api/ivr-event', (req, res) => {
    const { event, data } = req.body;
    if (!event || !data) return res.status(400).json({ error: 'bad payload' });
    const id = String(data.callId ?? '0');
    console.log(`[IVR] ${event}`, JSON.stringify(data));

    switch (event) {

        case 'call_answered': {
            const rawPhone = data.phone || parsePhone(data.remoteInfo || '');
            if (!calls[id]) {
                calls[id] = {
                    callId: id, state: 'ACTIVE',
                    phone: rawPhone, phoneDisplay: formatPhone(rawPhone),
                    remoteInfo: data.remoteInfo || '',
                    startTime: now(), profile: null, label: null,
                    currentStep: 0, totalSteps: 0,
                    stepResults: {}, history: [{ time: now(), event: '📞 Appel décroché' }]
                };
            }
            broadcast({ type: 'call_new', call: calls[id] });
            break;
        }

        case 'ivr_started':
            ensure(id, data);
            calls[id].profile     = data.profile;
            calls[id].label       = data.label;
            calls[id].state       = 'PLAYING';
            calls[id].totalSteps  = data.totalSteps;
            calls[id].currentStep = 0;
            if (data.remoteInfo && !calls[id].phone) {
                const rp = parsePhone(data.remoteInfo);
                calls[id].phone = rp;
                calls[id].phoneDisplay = formatPhone(rp);
            }
            calls[id].history.push({ time: now(), event: `▶ IVR — ${data.label}` });
            broadcast({ type: 'call_updated', call: calls[id] });
            break;

        case 'state_change':
            ensure(id, data);
            if (data.state !== 'IDLE') calls[id].state = data.state;
            broadcast({ type: 'state_change', callId: id, data });
            break;

        case 'step_started':
            ensure(id, data);
            calls[id].currentStep = data.stepIndex;
            calls[id].totalSteps  = data.totalSteps;
            if (!calls[id].stepResults[data.stepId])
                calls[id].stepResults[data.stepId] = { label: data.stepLabel, value: '', status: 'current' };
            else
                calls[id].stepResults[data.stepId].status = 'current';
            calls[id].history.push({ time: now(), event: `▶ Étape ${data.stepIndex+1}: ${data.stepLabel}` });
            broadcast({ type: 'step_started', callId: id, data });
            break;

        case 'dtmf_digit':
            ensure(id, data);
            if (!calls[id].stepResults[data.stepId])
                calls[id].stepResults[data.stepId] = { label: data.stepId, value: '', status: 'current' };
            calls[id].stepResults[data.stepId].value  = data.collected;
            calls[id].stepResults[data.stepId].status = 'current';
            broadcast({ type: 'dtmf_digit', callId: id, data });
            break;

        case 'step_reset':
            ensure(id, data);
            if (calls[id].stepResults[data.stepId]) {
                calls[id].stepResults[data.stepId].value  = '';
                calls[id].stepResults[data.stepId].status = 'current';
            }
            calls[id].history.push({ time: now(), event: `↺ Remise à zéro: ${data.stepId}` });
            broadcast({ type: 'step_reset', callId: id, data });
            break;

        case 'step_validated':
            ensure(id, data);
            if (!calls[id].stepResults[data.stepId])
                calls[id].stepResults[data.stepId] = { label: data.stepLabel, value: '', status: 'done' };
            calls[id].stepResults[data.stepId].value  = data.value;
            calls[id].stepResults[data.stepId].status = 'done';
            calls[id].history.push({ time: now(), event: `✓ ${data.stepLabel}: ${data.value}` });
            broadcast({ type: 'step_validated', callId: id, data });
            break;

        case 'step_replayed':
            ensure(id, data);
            calls[id].history.push({ time: now(), event: `↺ Agent rejoue étape ${data.stepIndex+1}` });
            broadcast({ type: 'step_replayed', callId: id, data });
            break;

        case 'step_skipped':
            ensure(id, data);
            calls[id].history.push({ time: now(), event: `⏭ Agent passe étape ${data.stepIndex+1}` });
            broadcast({ type: 'step_skipped', callId: id, data });
            break;

        case 'sequence_complete':
            ensure(id, data);
            calls[id].history.push({ time: now(), event: '📋 Séquence complète' });
            broadcast({ type: 'sequence_complete', callId: id, data });
            break;

        case 'call_hold':
            ensure(id, data);
            calls[id].state = 'HOLD';
            calls[id].history.push({ time: now(), event: '⏸ En attente (hold)' });
            broadcast({ type: 'call_hold', callId: id, data });
            break;

        case 'audio_done':
            ensure(id, data);
            broadcast({ type: 'audio_done', callId: id, data });
            break;

        case 'ivr_call_dropped':
            ensure(id, data);
            calls[id].state = 'DROPPED';
            calls[id].history.push({ time: now(), event: '📵 Appel raccroché' });
            broadcast({ type: 'call_dropped', callId: id });
            setTimeout(() => { archiveCall(id); removeCall(id); }, 5000);
            break;

        case 'call_ended':
            ensure(id, data);
            calls[id].state = 'DONE';
            calls[id].history.push({ time: now(), event: '✅ Appel terminé' });
            broadcast({ type: 'call_done', callId: id });
            setTimeout(() => { archiveCall(id); removeCall(id); }, 10000);
            break;

        case 'ivr_warn_wav_missing':
            ensure(id, data);
            calls[id].history.push({ time: now(), event: `⚠ WAV manquant: ${data.file}` });
            broadcast({ type: 'warn', callId: id, msg: `WAV manquant: ${data.file}` });
            break;

        case 'ivr_already_active':
            broadcast({ type: 'warn', callId: id, msg: 'IVR déjà actif' });
            break;
    }
    res.json({ ok: true });
});

app.post('/api/unhold/:callId', (req, res) => {
    const id = req.params.callId;
    if (calls[id]) {
        calls[id].history.push({ time: now(), event: 'Unhold depuis le panel' });
        broadcast({ type: 'unhold_requested', callId: id });
    }
    res.json({ ok: true });
});

// ── API Historique ────────────────────────────────────────────────────────────
app.get('/api/history', (req, res) => {
    res.json(getHistory(200));
});

app.get('/api/history/search', (req, res) => {
    const q = `%${req.query.q || ''}%`;
    if (!stmtSearchHistory) return res.json([]);
    const rows = stmtSearchHistory.all(q, q, q).map(row => ({
        ...row,
        stepResults: JSON.parse(row.stepResults || '{}'),
        history: JSON.parse(row.eventHistory || '[]')
    }));
    res.json(rows);
});

app.delete('/api/history', (req, res) => {
    if (stmtClearHistory) stmtClearHistory.run();
    broadcast({ type: 'history_updated', history: [] });
    res.json({ ok: true });
});

// ── Utilitaires ───────────────────────────────────────────────────────────────
function removeCall(id) {
    delete calls[id];
    broadcast({ type: 'call_removed', callId: id });
}

function ensure(id, data) {
    if (!calls[id]) {
        const rp = (data && data.phone) || '';
        calls[id] = {
            callId: id, state: 'ACTIVE',
            phone: rp, phoneDisplay: formatPhone(rp),
            remoteInfo: '', startTime: now(),
            profile: null, label: null,
            currentStep: 0, totalSteps: 1,
            stepResults: {}, history: []
        };
    }
}

function now() {
    return new Date().toLocaleTimeString('fr-CA',
        { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

const PORT = process.env.IVR_PORT || 3000;
server.listen(PORT, () => {
    console.log(`✅ IVR Live Panel v1.4 sur http://localhost:${PORT}`);
    console.log(`📂 Base de données: ${DB_PATH}`);
});
