/**
 * NIMCP Node.js Bindings - Module Loader
 * Tries cmake build path first, then node-gyp Release path.
 */

const path = require('path');

let nimcp;

// Try cmake build path (when built via cmake/node-gyp from project root)
const paths = [
    path.join(__dirname, 'build', 'Release', 'nimcp_nodejs.node'),
    path.join(__dirname, 'build', 'Debug', 'nimcp_nodejs.node'),
    // Legacy name
    path.join(__dirname, 'build', 'Release', 'nimcp.node'),
    path.join(__dirname, 'build', 'Debug', 'nimcp.node'),
];

for (const p of paths) {
    try {
        nimcp = require(p);
        break;
    } catch (e) {
        // try next
    }
}

if (!nimcp) {
    throw new Error(
        'NIMCP native module not found. Build with: cd src/bindings/nodejs && node-gyp rebuild\n' +
        'Searched paths:\n' + paths.map(p => '  ' + p).join('\n')
    );
}

module.exports = nimcp;
