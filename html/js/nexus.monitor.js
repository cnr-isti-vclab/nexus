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
	top:5px; right:5px; 
	opacity:0.7; 
	background-color:black; 
	color:white;
	padding:10px 20px;
	width:300px; 
}
#nexusMonitor table, #monitor tr { color:white; width:100% }
#nexusMonitor p { margin:0px 10px; padding:5px 0px; }

.progress {
	color:black;
	height: 1.5em;
	width: 100%;
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
}`

//INJECT STYLE IN THE HEADER

let style = document.createElement('STYLE');
style.type = 'text/css';
style.appendChild(document.createTextNode(css));
document.head.appendChild(style);


var nexusMonitor = null;
var nexusMonitorInterval = null;
var updateNexusMonitor = function() {}

document.addEventListener('DOMContentLoaded', function(){ 
	nexusMonitor = document.createElement('div');
	nexusMonitor.setAttribute('id', 'nexusMonitor');

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
		<tr><td>Rendered</td>
			<td><span id="nexusMonitorTriangles"></span></td></tr>
		<tr><td></td>
			<td><input type="checkbox" id="nexusMonitorNodes"> Show patches</td></tr>
		<tr><td>Target Error</td>
			<td><input type="number" min="0" max="100" id="nexusMonitorEditError"> px</td></tr>

	</table>`;

	nexusMonitor.innerHTML = html;

	let cacheLabel = nexusMonitor.querySelector('#nexusMonitorCache');
	let cacheStyle = nexusMonitor.querySelector('#nexusMonitorCache > span');
	let errorLabel = nexusMonitor.querySelector('#nexusMonitorError');
	let errorStyle = nexusMonitor.querySelector('#nexusMonitorError > span');
	let fpsLabel   = nexusMonitor.querySelector('#nexusMonitorFps');
	let fpsStyle   = nexusMonitor.querySelector('#nexusMonitorFps > span');

	let trianglesCount = nexusMonitor.querySelector('#nexusMonitorTriangles');
	let nodesCheckbox  = nexusMonitor.querySelector('#nexusMonitorNodes');
	let editError      = nexusMonitor.querySelector('#nexusMonitorEditError');

	nodesCheckbox.addEventListener('click', function() { 
		Nexus.Debug.nodes = this.checked; });

	editError.addEventListener('change', function() { 
		if((typeof(Nexus) === 'undefined') || Nexus.contexts.length == 0) return;
		Nexus.contexts[0].targetError = this.value;
	});

	document.body.appendChild(nexusMonitor);

	const greenToRed = (percent) => {
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

	updateNexusMonitor = function () {
		if((typeof(Nexus) === 'undefined') || Nexus.contexts.length == 0) return;

		let context = Nexus.contexts[0];

		let cacheSize = context.cacheSize;
		let maxCacheSize = context.maxCacheSize;
		let cacheFraction = parseInt(100*cacheSize/maxCacheSize);

		let targetError = context.targetError;
		let realError = context.realError;
		let currentError = context.currentError;
		let errorFraction = 100*Math.min(5, Math.log2(currentError/targetError))/5;

		let minFps = context.minFps;
		let currentFps = context.currentFps;
		let fpsFraction = 100*(Math.min(3, Math.max(0, 60/currentFps  -1)))/3;

		let rendered = context.rendered;

		cacheLabel.setAttribute('data-label', `${toHuman(cacheSize, 'B')}/${toHuman(maxCacheSize, 'B')}`);
		cacheStyle.style.background_color = greenToRed(cacheFraction);
		cacheStyle.style.width = cacheFraction;

		errorLabel.setAttribute('data-label', `Real: ${realError.toFixed(1)} px  Current: ${currentError.toFixed(1)} px`);
		errorStyle.style.background_color = greenToRed(errorFraction);
		errorStyle.style.width = errorFraction;

		fpsLabel.setAttribute('data-label', `${parseInt(Math.round(currentFps))} fps`);
		fpsStyle.style.background_color = greenToRed(fpsFraction);
		fpsStyle.style.width = fpsFraction;

		trianglesCount.innerHTML = `${toHuman(rendered)} triangles`;

		if(!editError.value)
			editError.value = targetError;
	}

	startNexusMonitor();
}, false);

function startNexusMonitor() {
	stopNexusMonitor();
	nexusMonitor.style.display = 'block';
	nexusMonitorInterval = setInterval(updateNexusMonitor, 100);
}

function stopNexusMonitor() {
	nexusMonitor.style.display = 'none';
	if(nexusMonitorInterval)
		clearInterval(nexusMonitorInterval);
}




