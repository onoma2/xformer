import ModuleFactory from './tuesday.js';

const STEP_SIZE = 104; // bytes per ExportedStep (packed layout in tuesday_wasm.cpp)

const controls = [
  { id: 'algo', key: 0 },
  { id: 'flow', key: 1 },
  { id: 'ornament', key: 2 },
  { id: 'power', key: 3 },
  { id: 'glide', key: 4 },
  { id: 'stepTrill', key: 5 },
  { id: 'gateLength', key: 7 },
  { id: 'gateOffset', key: 8 },
];

let Module = null;

const gridEl = document.getElementById('grid');
const jsonEl = document.getElementById('json');

function attachValueMirrors() {
  document.querySelectorAll('.control input[type="range"]').forEach((el) => {
    const span = el.parentElement.querySelector('.value');
    const update = () => { span.textContent = el.value; };
    el.addEventListener('input', update);
    update();
  });
}

function setParamsFromUI() {
  controls.forEach(({ id, key }) => {
    const el = document.getElementById(id);
    const value = el.tagName === 'SELECT' ? parseInt(el.value, 10) : Number(el.value);
    Module.ccall('wasm_set_param', null, ['number', 'number'], [key, value]);
  });
}

function readSteps() {
  const count = Module.ccall('wasm_get_steps_len', 'number', [], []);
  const ptr = Module.ccall('wasm_get_steps_ptr', 'number', [], []);
  if (!ptr || count === 0) return [];
  const view = new DataView(Module.HEAPU8.buffer, ptr, count * STEP_SIZE);
  const steps = [];
  for (let i = 0; i < count; i++) {
    const base = i * STEP_SIZE;
    const step = {
      stepIndex: view.getUint32(base, true),
      tickOn: view.getUint32(base + 4, true),
      tickOff: view.getUint32(base + 8, true),
      cv: view.getFloat32(base + 12, true),
      note: view.getInt32(base + 16, true),
      octave: view.getInt32(base + 20, true),
      velocity: view.getUint8(base + 24),
      accent: view.getUint8(base + 25),
      slide: view.getUint8(base + 26),
      gateRatio: view.getUint8(base + 27),
      gateOffset: view.getUint8(base + 28),
      polyCount: view.getUint8(base + 29),
      microCount: view.getUint8(base + 30),
      microTicks: [],
      microCv: [],
      noteOffsets: [],
    };
    const microTicksOff = base + 32;
    const microCvOff = base + 64;
    const noteOffsetsOff = base + 96;
    for (let m = 0; m < 8; m++) {
      step.microTicks.push(view.getUint32(microTicksOff + m * 4, true));
      step.microCv.push(view.getFloat32(microCvOff + m * 4, true));
      step.noteOffsets.push(view.getInt8(noteOffsetsOff + m));
    }
    steps.push(step);
  }
  return steps;
}

function renderGrid(steps) {
  gridEl.innerHTML = '';
  steps.forEach((s) => {
    const card = document.createElement('div');
    card.className = 'step-card';
    const head = document.createElement('div');
    head.className = 'head';
    head.innerHTML = `<span>#${s.stepIndex + 1}</span><span class="cv">${s.cv.toFixed(3)}V</span>`;
    const note = document.createElement('div');
    note.className = 'note';
    note.textContent = `note ${s.note} / oct ${s.octave}`;
    const vel = document.createElement('div');
    vel.style.color = 'var(--muted)';
    vel.style.fontSize = '11px';
    vel.textContent = `vel ${s.velocity} • gate ${s.gateRatio}% • offs ${s.gateOffset}%`;
    const micro = document.createElement('div');
    micro.className = 'micro';
    const chips = Math.max(1, s.polyCount || s.microCount || 1);
    for (let i = 0; i < chips; i++) {
      const c = document.createElement('div');
      c.className = 'chip';
      if (s.microCount > 0 && i < s.microCount) c.classList.add('on');
      if (s.accent) c.classList.add('accent');
      micro.appendChild(c);
    }
    card.append(head, note, vel, micro);
    gridEl.appendChild(card);
  });
}

async function main() {
  Module = await ModuleFactory();
  Module.ccall('wasm_init', null, [], []);
  attachValueMirrors();

  const run = () => {
    setParamsFromUI();
    Module.ccall('wasm_run_steps', 'number', ['number'], [32]);
    const steps = readSteps();
    renderGrid(steps);
    jsonEl.textContent = JSON.stringify(steps, null, 2);
  };

  document.getElementById('run-btn').addEventListener('click', run);
  run();
}

main();
