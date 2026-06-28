// server.js — Live Panel IVR v1.5 (sql.js — SQLite pur JS, zero compilation)
const express   = require('express');
const http      = require('http');
const WebSocket = require('ws');
const path      = require('path');
const fs        = require('fs');
const initSqlJs = require('sql.js');

const app    = express();
const server = http.createServer(app);
const wss    = new WebSocket.Server({ server });

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

const DB_PATH = process.env.IVR_DATA_PATH
    ? path.join(process.env.IVR_DATA_PATH, 'ivr_history.db')
    : 'C:\\IVR\\ivr_history.db';

let db = null;

function saveDb() {
    if (!db) return;
    try { fs.writeFileSync(DB_PATH, Buffer.from(db.export())); }
    catch(e) { console.error('[DB] save error:', e.message); }
}

function dbRun(sql, params=[]) {
    if (!db) return;
    try { db.run(sql, params); } catch(e) { console.error('[DB] run:', e.message); }
}

function dbAll(sql, params=[]) {
    if (!db) return [];
    try {
        const stmt = db.prepare(sql);
        stmt.bind(params);
        const rows = [];
        while (stmt.step()) rows.push(stmt.getAsObject());
        stmt.free();
        return rows;
    } catch(e) { console.error('[DB] query:', e.message); return []; }
}

function getHistory(limit=200) {
    return dbAll('SELECT * FROM call_history ORDER BY id DESC LIMIT ?', [limit]).map(r => ({
        ...r,
        stepResults: JSON.parse(r.stepResults||'{}'),
        history:     JSON.parse(r.eventHistory||'[]')
    }));
}

function archiveCall(id) {
    if (!db || !calls[id]) return;
    const c = calls[id];
    const hasData = Object.keys(c.stepResults||{}).some(k => c.stepResults[k].value);
    if (!hasData && !['DONE','DROPPED','HOLD'].includes(c.state)) return;
    try {
        dbRun(`INSERT INTO call_history
            (callId,phone,phoneDisplay,label,profile,startTime,archivedAt,finalState,stepResults,eventHistory)
            VALUES (?,?,?,?,?,?,?,?,?,?)`,
            [c.callId,c.phone||'',c.phoneDisplay||'',c.label||'',c.profile||'',
             c.startTime||'',now(),c.state||'DONE',
             JSON.stringify(c.stepResults||{}),
             JSON.stringify((c.history||[]).slice(-20))]);
        saveDb();
        broadcast({ type:'history_updated', history:getHistory(100) });
        console.log(`[DB] Archive: ${c.phoneDisplay||c.callId}`);
    } catch(e) { console.error('[DB] archive:', e.message); }
}

const calls = {};

function broadcast(data) {
    const msg = JSON.stringify(data);
    wss.clients.forEach(c => { if (c.readyState===WebSocket.OPEN) c.send(msg); });
}

function parsePhone(r) {
    if (!r) return '';
    let m = r.match(/sip:([0-9+*#]{4,20})@/);
    if (m) return m[1];
    m = r.match(/([0-9]{7,20})/);
    return m ? m[1] : r;
}

function formatPhone(n) {
    const d=(n||'').replace(/\D/g,'');
    if (d.length===11&&d[0]==='1') return `+1 (${d.slice(1,4)}) ${d.slice(4,7)}-${d.slice(7)}`;
    if (d.length===10) return `(${d.slice(0,3)}) ${d.slice(3,6)}-${d.slice(6)}`;
    return n;
}

wss.on('connection', ws => {
    ws.send(JSON.stringify({ type:'init', calls, history:getHistory(100) }));
});

app.post('/api/ivr-event', (req, res) => {
    const {event, data} = req.body;
    if (!event||!data) return res.status(400).json({error:'bad payload'});
    const id = String(data.callId??'0');
    console.log(`[IVR] ${event}`, JSON.stringify(data));

    switch (event) {
        case 'call_answered': {
            const rp=data.phone||parsePhone(data.remoteInfo||'');
            if (!calls[id]) calls[id]={
                callId:id,state:'ACTIVE',phone:rp,phoneDisplay:formatPhone(rp),
                remoteInfo:data.remoteInfo||'',startTime:now(),
                profile:null,label:null,currentStep:0,totalSteps:0,
                stepResults:{},history:[{time:now(),event:'Appel decroché'}]
            };
            broadcast({type:'call_new',call:calls[id]}); break;
        }
        case 'ivr_started':
            ensure(id,data);
            calls[id].profile=data.profile; calls[id].label=data.label;
            calls[id].state='PLAYING'; calls[id].totalSteps=data.totalSteps; calls[id].currentStep=0;
            if (data.remoteInfo&&!calls[id].phone){const rp=parsePhone(data.remoteInfo);calls[id].phone=rp;calls[id].phoneDisplay=formatPhone(rp);}
            calls[id].history.push({time:now(),event:`IVR — ${data.label}`});
            broadcast({type:'call_updated',call:calls[id]}); break;
        case 'state_change':
            ensure(id,data); if(data.state!=='IDLE')calls[id].state=data.state;
            broadcast({type:'state_change',callId:id,data}); break;
        case 'step_started':
            ensure(id,data); calls[id].currentStep=data.stepIndex; calls[id].totalSteps=data.totalSteps;
            if(!calls[id].stepResults[data.stepId]) calls[id].stepResults[data.stepId]={label:data.stepLabel,value:'',status:'current'};
            else calls[id].stepResults[data.stepId].status='current';
            calls[id].history.push({time:now(),event:`Etape ${data.stepIndex+1}: ${data.stepLabel}`});
            broadcast({type:'step_started',callId:id,data}); break;
        case 'dtmf_digit':
            ensure(id,data);
            if(!calls[id].stepResults[data.stepId]) calls[id].stepResults[data.stepId]={label:data.stepId,value:'',status:'current'};
            calls[id].stepResults[data.stepId].value=data.collected; calls[id].stepResults[data.stepId].status='current';
            broadcast({type:'dtmf_digit',callId:id,data}); break;
        case 'step_reset':
            ensure(id,data);
            if(calls[id].stepResults[data.stepId]){calls[id].stepResults[data.stepId].value='';calls[id].stepResults[data.stepId].status='current';}
            calls[id].history.push({time:now(),event:`Reset: ${data.stepId}`});
            broadcast({type:'step_reset',callId:id,data}); break;
        case 'step_validated':
            ensure(id,data);
            if(!calls[id].stepResults[data.stepId]) calls[id].stepResults[data.stepId]={label:data.stepLabel,value:'',status:'done'};
            calls[id].stepResults[data.stepId].value=data.value; calls[id].stepResults[data.stepId].status='done';
            calls[id].history.push({time:now(),event:`OK ${data.stepLabel}: ${data.value}`});
            broadcast({type:'step_validated',callId:id,data}); break;
        case 'step_back':
            ensure(id,data); calls[id].history.push({time:now(),event:`Retour etape ${data.stepIndex+1}`});
            broadcast({type:'step_back',callId:id,data}); break;
        case 'step_replayed':
            ensure(id,data); calls[id].history.push({time:now(),event:`Rejouer etape ${data.stepIndex+1}`});
            broadcast({type:'step_replayed',callId:id,data}); break;
        case 'step_skipped':
            ensure(id,data); calls[id].history.push({time:now(),event:`Passer etape ${data.stepIndex+1}`});
            broadcast({type:'step_skipped',callId:id,data}); break;
        case 'sequence_complete':
            ensure(id,data); calls[id].history.push({time:now(),event:'Sequence complete'});
            broadcast({type:'sequence_complete',callId:id,data}); break;
        case 'call_hold':
            ensure(id,data); calls[id].state='HOLD'; calls[id].history.push({time:now(),event:'En attente (hold)'});
            broadcast({type:'call_hold',callId:id,data}); break;
        case 'audio_done':
            ensure(id,data); broadcast({type:'audio_done',callId:id,data}); break;
        case 'ivr_finale_playing':
            ensure(id,data); broadcast({type:'ivr_finale_playing',callId:id,data}); break;
        case 'ivr_call_dropped':
            ensure(id,data); calls[id].state='DROPPED'; calls[id].history.push({time:now(),event:'Appel raccroche'});
            broadcast({type:'call_dropped',callId:id});
            setTimeout(()=>{archiveCall(id);removeCall(id);},5000); break;
        case 'call_ended':
            ensure(id,data); calls[id].state='DONE'; calls[id].history.push({time:now(),event:'Appel termine'});
            broadcast({type:'call_done',callId:id});
            setTimeout(()=>{archiveCall(id);removeCall(id);},10000); break;
        case 'ivr_warn_wav_missing':
            ensure(id,data); calls[id].history.push({time:now(),event:`WAV manquant: ${data.file}`});
            broadcast({type:'warn',callId:id}); break;
    }
    res.json({ok:true});
});

app.post('/api/unhold/:callId', (req,res) => {
    const id=req.params.callId;
    if(calls[id]){calls[id].history.push({time:now(),event:'Unhold depuis panel'});broadcast({type:'unhold_requested',callId:id});}
    res.json({ok:true});
});

app.get('/api/history', (req,res) => res.json(getHistory(200)));
app.get('/api/history/search', (req,res) => {
    const q=`%${req.query.q||''}%`;
    res.json(dbAll('SELECT * FROM call_history WHERE phone LIKE ? OR stepResults LIKE ? ORDER BY id DESC LIMIT 50',[q,q])
        .map(r=>({...r,stepResults:JSON.parse(r.stepResults||'{}'),history:JSON.parse(r.eventHistory||'[]')})));
});
app.delete('/api/history', (req,res) => {
    dbRun('DELETE FROM call_history'); saveDb();
    broadcast({type:'history_updated',history:[]});
    res.json({ok:true});
});

function removeCall(id){delete calls[id];broadcast({type:'call_removed',callId:id});}
function ensure(id,data){
    if(!calls[id]){const rp=(data&&data.phone)||'';
    calls[id]={callId:id,state:'ACTIVE',phone:rp,phoneDisplay:formatPhone(rp),
    remoteInfo:'',startTime:now(),profile:null,label:null,currentStep:0,totalSteps:1,stepResults:{},history:[]};}
}
function now(){return new Date().toLocaleTimeString('fr-CA',{hour:'2-digit',minute:'2-digit',second:'2-digit'});}

// ── Init async (sql.js = WebAssembly, init requise) ───────────────────────────
const PORT = process.env.IVR_PORT || 3000;

initSqlJs().then(SQL => {
    try {
        db = fs.existsSync(DB_PATH)
            ? new SQL.Database(fs.readFileSync(DB_PATH))
            : new SQL.Database();
        console.log(fs.existsSync(DB_PATH) ? `[DB] Charge: ${DB_PATH}` : `[DB] Nouvelle base: ${DB_PATH}`);
    } catch(e) {
        db = new SQL.Database();
        console.warn(`[DB] Erreur, nouvelle base: ${e.message}`);
    }

    dbRun(`CREATE TABLE IF NOT EXISTS call_history (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        callId TEXT DEFAULT '', phone TEXT DEFAULT '', phoneDisplay TEXT DEFAULT '',
        label TEXT DEFAULT '', profile TEXT DEFAULT '',
        startTime TEXT DEFAULT '', archivedAt TEXT DEFAULT '',
        finalState TEXT DEFAULT 'DONE', stepResults TEXT DEFAULT '{}', eventHistory TEXT DEFAULT '[]'
    )`);
    saveDb();

    server.listen(PORT, () => {
        console.log(`OK IVR Live Panel v1.5 sur http://localhost:${PORT}`);
        console.log(`DB: ${DB_PATH}`);
    });
}).catch(err => { console.error('ERREUR init:', err); process.exit(1); });
