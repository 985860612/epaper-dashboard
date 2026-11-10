const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const page = fs.readFileSync(path.join(__dirname, '../firmware/epaper_dashboard/web_page.h'), 'utf8');
const drawSource = page.split('\n').find(line => line.startsWith('function draw(s)'));
function render(points, previous=30, opening=28) {
  const calls=[];
  const context = Object.fromEntries(['clearRect','setLineDash','beginPath','moveTo','lineTo'].map(name=>[name,(...args)=>calls.push([name,...args])]));
  context.stroke=function(){calls.push(['stroke',this.strokeStyle]);};
  const draw = new Function('$',drawSource+'; return draw;')(()=>({width:760,height:160,getContext:()=>context}));
  draw({previous,open:opening,points});return calls;
}
const points=[[0,29,29.60],[120,31,29.65],[240,29,29.71]];
const original=render(points);
assert.deepEqual(original,render(points.map(([t,p])=>[t,p,9999])));
assert.deepEqual(original.filter(c=>c[0]==='stroke').map(c=>c[1]),['#111','#111','#b0162b','#b0162b','#111']);
const strokes=original.filter(c=>c[0]==='stroke');assert.equal(strokes.length,5);
// Crossings are halfway along the two price segments, not at sample boundaries.
assert(original.some(c=>c[0]==='lineTo' && c[1]===193));assert(original.some(c=>c[0]==='lineTo' && c[1]===567));
assert.deepEqual(render([[0,30],[240,30]]).filter(c=>c[0]==='stroke').map(c=>c[1]),['#111','#111']);
assert.deepEqual(render([[0,31],[240,32]]).filter(c=>c[0]==='stroke').map(c=>c[1]),['#111','#b0162b']);
assert.deepEqual(render(points,0),[['clearRect',0,0,760,160]]);
assert.deepEqual(render(points,NaN),[['clearRect',0,0,760,160]]);
assert.deepEqual(original,render(points,30,0));assert.deepEqual(original,render(points,30,100));
new Function(page.match(/<script>([\s\S]*?)<\/script>/)[1]);
console.log('PASS: embedded web JavaScript syntax, previous-close color threshold, rising/falling crossings, equal/missing previous close, no average influence on line or range');
