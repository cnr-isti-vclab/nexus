import { terser } from "rollup-plugin-terser";

function header() {
	return {

		renderChunk( code ) {
			return "// vcg.isti.cnr.it/nexus/license\n" + code;
		}
	};

}

export default [
	{
		input: './Nexus3D.js',
		output: [{
			format: 'umd',
			name: 'Nexus3D',
			file: 'build/nexus3D.min.js',
			plugins: [terser()],
			globals: { three: 'THREE' }
		},
		{
			format: 'umd',
			name: 'Nexus3D',
			file: 'build/nexus3D.js',
			globals: { three: 'THREE' }
		}],
		external: [ 'three' ]
	}
	
];
