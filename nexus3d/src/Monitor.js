/*
Nexus
Copyright (c) 2012-2020, Visual Computing Lab, ISTI - CNR
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

let css = `
#nexusMonitor {
	position:absolute; 
	top:10px; left:10px; 
	opacity:0.7; 
	background-color:black; 
	color:white;
	padding:10px 20px;
	width:300px; 
	font-family:Arial;
	font-size: 12px;
}
#nexusMonitor table, #monitor tr { color:white; }
#nexusMonitor p { margin:0px 10px; padding:5px 0px; }

.progress {
	color:black;
	height: 1.5em;
	width: 240px;
	background-color: #c9c9c9;
	position: relative;
}
.progress:before {
	content: attr(data-label);
	font-size: 0.8em;
	position: absolute;
	text-align: center;
	top: 5px;
	left: 0;
	right: 0;
}
.progress .value {
	background-color: #7cc4ff;
	display: inline-block;
	height: 100%;
}
#nexusMonitor canvas {
	width:300px;
	height:600px;
	background: transparent;
}
`


let html = `<table>
<tr><td style="width:30%">Cache</td>
	<td style="width:70%"><div class="progress" id="nexusMonitorCache" data-label="">
		<span class="value"></span>
	</div></td></tr>
<tr><td>Error</td>
	<td style="width:70%"><div class="progress" id="nexusMonitorError" data-label="">
		<span class="value"></span>
	</div></td></tr>

<tr><td>Fps</td>
	<td style="width:70%"><div class="progress" id="nexusMonitorFps" data-label="">
		<span class="value"></span>
	</div></td></tr>

<tr><td>Progress</td>
	<td style="width:70%"><div class="progress" id="nexusMonitorProgress" data-label="">
		<span class="value"></span>
	</div></td></tr>

<tr><td>Rendered</td>
	<td><span id="nexusMonitorTriangles"></span></td></tr>
<tr><td></td>
	<td><input type="checkbox" id="nexusMonitorNodes"> Show patches</td></tr>
<tr><td>Target Error</td>
	<td><input type="number" min="0" max="100" id="nexusMonitorEditError"> px</td></tr>

</table>
<canvas width="300" height="600"></canvas>
`;

//INJECT STYLE IN THE HEADER


function greenToRed(percent) {
	const r = 255 * percent/100;
	const g = 255 - (255 * percent/100);
	return 'rgb('+r+','+g+',0)';
}

function toHuman(bytes, unit = '', decimals = 1) {
	if (bytes === 0) return '0 Bytes';

	const k = 1024;
	const dm = decimals < 0 ? 0 : decimals;
	const sizes = ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];

	const i = Math.floor(Math.log(bytes) / Math.log(k));
	return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i] + unit;
}

class Monitor {

	constructor(cache, model) {
		this.cache = cache;
		this.div = null;
		this.interval = null;
		this.model = model;
		this.mesh = model.mesh;

		document.addEventListener('DOMContentLoaded', () => {

			let style = document.createElement('STYLE');
			style.type = 'text/css';
			style.appendChild(document.createTextNode(css));
			document.head.appendChild(style);


			this.div = document.createElement('div');
			this.div.setAttribute('id', 'nexusMonitor');

			this.div.innerHTML = html;
			let e = this.elements = {
				cacheLabel: this.div.querySelector('#nexusMonitorCache'),
				cacheStyle: this.div.querySelector('#nexusMonitorCache > span'),
				errorLabel: this.div.querySelector('#nexusMonitorError'),
				errorStyle: this.div.querySelector('#nexusMonitorError > span'),
				fpsLabel  : this.div.querySelector('#nexusMonitorFps'),
				fpsStyle  : this.div.querySelector('#nexusMonitorFps > span'),
				progressLabel  : this.div.querySelector('#nexusMonitorProgress'),
				progressStyle  : this.div.querySelector('#nexusMonitorProgress > span'),
				

				trianglesCount: this.div.querySelector('#nexusMonitorTriangles'),
				nodesCheckbox : this.div.querySelector('#nexusMonitorNodes'),
				editError     : this.div.querySelector('#nexusMonitorEditError')
			}

			let cache = this.cache;
			e.nodesCheckbox.addEventListener('click', function() { 
				cache.debug.nodes = this.checked; 
				for( let [mesh, ids] of cache.nodes) {
					for(let  callback of mesh.onUpdate)
						callback(mesh);
				}
			});

			e.editError.addEventListener('change', function() { 
				cache.targetError = this.value;
				for( let [mesh, ids] of cache.nodes) {
					for(let  callback of mesh.onUpdate)
						callback(mesh);
				}
			});

			document.body.appendChild(this.div);
			this.canvas = this.div.querySelector('canvas');
			this.ctx = this.canvas.getContext('2d');


			this.start();
		}, false);
	}

	update() {
		let context = this.cache;

		let cacheSize = context.cacheSize;
		let maxCacheSize = context.maxCacheSize;
		let cacheFraction = parseInt(100*cacheSize/maxCacheSize);

		let targetError = context.targetError;
		let realError = Math.min(1000, context.realError);
		let currentError = context.currentError;
		let errorFraction = 100*Math.min(5, Math.log2(currentError/targetError))/5;

		let minFps = context.minFps;
		let currentFps = context.currentFps;
		let fpsFraction = 100*(Math.min(3, Math.max(0, 60/currentFps  -1)))/3;

		
		let mesh = context.nodes.keys().next();
		if(mesh) mesh = mesh.value;
		
		let progressFraction = mesh? mesh.availableNodes / mesh.nodesCount : 0;
		let progressNodes = mesh ? mesh.availableNodes : 0;

		let rendered = context.rendered;
		
		let e = this.elements;

		e.cacheLabel.setAttribute('data-label', `${toHuman(cacheSize, 'B')}/${toHuman(maxCacheSize, 'B')}`);
		e.cacheStyle.style.background_color = greenToRed(cacheFraction);
		e.cacheStyle.style.width = cacheFraction;

		e.errorLabel.setAttribute('data-label', `Real: ${realError.toFixed(1)} px  Target: ${currentError.toFixed(1)} px`);
		e.errorStyle.style.background_color = greenToRed(errorFraction);
		e.errorStyle.style.width = errorFraction;

		e.fpsLabel.setAttribute('data-label', `${parseInt(Math.round(currentFps))} fps`);
		e.fpsStyle.style.background_color = greenToRed(fpsFraction);
		e.fpsStyle.style.width = fpsFraction;

		e.progressLabel.setAttribute('data-label', `${parseInt(Math.round(progressNodes))} nodes`);
		e.progressStyle.style.background_color = greenToRed(progressFraction);
		e.progressStyle.style.width = progressFraction;

		e.trianglesCount.innerHTML = `${toHuman(rendered)} triangles`;

		if(!e.editError.value)
			e.editError.value = targetError;

		if(!this.levels) {
			if(this.mesh.isReady)
				this.buildLevels();
		} else
			this.updateNodeMonitor();
	}
	
	start() {
		this.stop();
		this.div.style.display = 'block';
		this.interval = setInterval(() => { this.update(); }, 100);
	}

	stop() {
		this.div.style.display = 'none';
		if(this.interval)
			clearInterval(this.interval);
	}

	buildLevels() {
		let m = this.mesh;
		this.levelsCount = 0;
		this.levels = new Array(m.nodesCount);
		this.levels.fill(-1);
		this.nodes = [];
		for(let n = 0; n < m.nodesCount-1; n++) {
			let level = this.levels[n];
			if(level == -1)
				level = this.levels[n] = 0;
			if(this.nodes.length <= level)
				this.nodes[level] = [];

			this.nodes[level].push(n);

			for(let p = m.nfirstpatch[n]; p < m.nfirstpatch[n+1]; p++) {
				let child = m.patches[p*3];
				if(child == m.nodesCount-1)
					continue;

				this.levels[child] = level+1;
			}
			this.levelsCount = Math.max(level, this.levelsCount);
		}
	}

	updateNodeMonitor() {

		let ctx = this.ctx;
		ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

		let h = 10;
		let w = 300;

		let y = 0;
		for(let level = 0; level < this.nodes.length; level++) {
			let nodes = this.nodes[level];
			let x = 0;
			let dx = 10;
			for(let i = 0; i < nodes.length; i++) {
				let n = nodes[i];
				let selected = this.model.traversal.selected[n];
				let status = this.mesh.status[n];
				let color = 'red';
				if(selected)
					color = 'green';
				else if(status == 1)
					color = 'yellow';
				else if(status > 1)
					color = 'orange';
				else
					color = 'red';
				//let color = status == 0 ? 'red' : (status == 1? 'green' : 'orange');


				ctx.beginPath();
				ctx.rect(x, y, dx, h);
				ctx.fillStyle = color;
				ctx.fill();
				x += dx + 1;
				if(x >= w - 10) {
					x = 0;
					y += 11;
				}
			}
			y += h + 3;
		}
	}
}


export { Monitor }
