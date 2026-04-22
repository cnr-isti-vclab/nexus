import { terser } from "rollup-plugin-terser";

// Helper for license header
const license = "// vcg.isti.cnr.it/nexus/license\n";

export default [
    // Build the Main Library (Nexus3D)
    {
        input: './src/Nexus3D.js',
        output: [
            {
                format: 'es',
                file: 'build/nexus3D.js',
            },
            {
                format: 'umd',
                name: 'Nexus3D',
                file: 'build/nexus3D.min.js',
                plugins: [terser()],
                globals: { three: 'THREE' }
            }
        ],
        external: ['three']
    },
    // Build the Worker (Corto)
    {
        input: './dist/corto.em.js', 
        output: {
            format: 'iife', // Classic worker format is safest for wide compatibility
            file: 'build/corto.worker.js',
            plugins: [
                terser(),
                { renderChunk: (code) => license + code }
            ]
        }
    }
];