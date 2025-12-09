// ILP Tuner Core Logic
// Extracted for testability
// Copyright (c) 2025 Matt Vanderdorff
// SPDX-License-Identifier: BSL-1.0

const TunerCore = (function() {
    'use strict';

    const GODBOLT_API = 'https://godbolt.org/api';

    /**
     * Wrap user code in a test harness function for MCA analysis
     * @param {string} userCode - The user's loop code
     * @param {string} arch - Target architecture
     * @returns {string} Complete wrapped source code
     */
    function wrapCode(userCode, arch) {
        if (typeof userCode !== 'string') {
            throw new TypeError('userCode must be a string');
        }
        if (typeof arch !== 'string') {
            throw new TypeError('arch must be a string');
        }

        return `#include <cstddef>
#include <cstdint>

// Prevent optimization of the result
template<typename T>
void escape(T&& val) {
    asm volatile("" : : "g"(val) : "memory");
}

// Prevent optimization of reads
void clobber() {
    asm volatile("" : : : "memory");
}

void analyze_loop(int* __restrict data, size_t n) {
    int sum = 0;
    int* __restrict ptr = data;

    ${userCode}

    escape(sum);
}
`;
    }

    /**
     * Get compiler architecture flag for given architecture
     * @param {string} arch - Architecture identifier
     * @returns {string} Compiler flag
     */
    function getArchFlag(arch) {
        if (typeof arch !== 'string') {
            return '-march=skylake';
        }
        switch(arch) {
            case 'skylake': return '-march=skylake';
            case 'znver4': return '-march=znver4';
            case 'apple-m1': return '-mcpu=apple-m1';
            case 'cortex-a72': return '-mcpu=cortex-a72';
            default: return '-march=skylake';
        }
    }

    /**
     * Get LLVM-MCA CPU flag for given architecture
     * @param {string} arch - Architecture identifier
     * @returns {string} MCA CPU flag
     */
    function getMcaCpu(arch) {
        if (typeof arch !== 'string') {
            return '-mcpu=skylake';
        }
        switch(arch) {
            case 'skylake': return '-mcpu=skylake';
            case 'znver4': return '-mcpu=znver4';
            case 'apple-m1': return '-mcpu=apple-m1';
            case 'cortex-a72': return '-mcpu=cortex-a72';
            default: return '-mcpu=skylake';
        }
    }

    /**
     * Parse LLVM-MCA output to extract metrics
     * @param {string} mcaOutput - Raw MCA output text
     * @returns {Object} Parsed metrics
     */
    function parseMetrics(mcaOutput) {
        const metrics = {};

        if (typeof mcaOutput !== 'string' || mcaOutput.length === 0) {
            return metrics;
        }

        // Block RThroughput - the key metric for determining unroll factor
        const rtMatch = mcaOutput.match(/Block RThroughput:\s*([\d.]+)/);
        if (rtMatch) {
            const parsed = parseFloat(rtMatch[1]);
            if (!isNaN(parsed) && isFinite(parsed)) {
                metrics.rthroughput = parsed;
            }
        }

        // Total Cycles
        const cyclesMatch = mcaOutput.match(/Total Cycles:\s*(\d+)/);
        if (cyclesMatch) {
            const parsed = parseInt(cyclesMatch[1], 10);
            if (!isNaN(parsed)) {
                metrics.cycles = parsed;
            }
        }

        // Bottleneck analysis
        const bottleneckMatch = mcaOutput.match(/Bottleneck:\s*([^\n]+)/);
        if (bottleneckMatch) {
            metrics.bottleneck = bottleneckMatch[1].trim();
        }

        // uOps Per Cycle
        const uopsMatch = mcaOutput.match(/IPC:\s*([\d.]+)/);
        if (uopsMatch) {
            const parsed = parseFloat(uopsMatch[1]);
            if (!isNaN(parsed) && isFinite(parsed)) {
                metrics.ipc = parsed;
            }
        }

        // Dispatch Width
        const dispatchMatch = mcaOutput.match(/Dispatch Width:\s*(\d+)/);
        if (dispatchMatch) {
            const parsed = parseInt(dispatchMatch[1], 10);
            if (!isNaN(parsed)) {
                metrics.dispatchWidth = parsed;
            }
        }

        // Parse Instruction Info table to extract per-instruction latency and rthroughput
        // Format:
        // [1]    [2]    [3]    Instructions:
        //  1      4     1.00   vaddss %xmm0, %xmm1, %xmm2
        const infoMatch = mcaOutput.match(/Instruction Info:[\s\S]*?Instructions:\s*\n([\s\S]*?)(?:\n\n|\nResource|$)/);
        if (infoMatch) {
            const lines = infoMatch[1].split('\n').filter(l => l.trim());
            metrics.instructions = [];
            for (const line of lines) {
                // Match: whitespace, uops, whitespace, latency, whitespace, rthroughput, whitespace, instruction
                const match = line.match(/^\s*(\d+)\s+(\d+)\s+([\d.]+)\s+(.+)$/);
                if (match) {
                    const lat = parseInt(match[2], 10);
                    const rt = parseFloat(match[3]);
                    const instr = match[4].trim();
                    if (!isNaN(lat) && !isNaN(rt)) {
                        metrics.instructions.push({
                            latency: lat,
                            rthroughput: rt,
                            instruction: instr
                        });
                    }
                }
            }
        }

        return metrics;
    }

    /**
     * Generate N recommendation based on MCA metrics
     * Formula: N = max(ceil(Li / RThr_i)) for each instruction i (clamped to [2, 16])
     * @param {Object} metrics - Parsed MCA metrics
     * @returns {Object} Recommendation with suggestedN, reasoning, and perInstruction breakdown
     */
    function generateRecommendation(metrics) {
        const reasoning = [];
        const perInstruction = [];
        let suggestedN = 4; // sensible default

        if (!metrics || typeof metrics !== 'object') {
            reasoning.push('No metrics available - using default N=4');
            return { suggestedN, reasoning, perInstruction };
        }

        // Use per-instruction formula: N = max(ceil(Li / RThr_i))
        if (metrics.instructions && Array.isArray(metrics.instructions) && metrics.instructions.length > 0) {
            let maxN = 2;
            for (const inst of metrics.instructions) {
                if (inst.rthroughput > 0) {
                    const n = Math.ceil(inst.latency / inst.rthroughput);
                    perInstruction.push({
                        instruction: inst.instruction,
                        latency: inst.latency,
                        rthroughput: inst.rthroughput,
                        n: n
                    });
                    maxN = Math.max(maxN, n);
                }
            }
            suggestedN = Math.max(2, Math.min(16, maxN));
            reasoning.push(`N = max(ceil(L/RThr) per instruction) = ${suggestedN}`);
        } else {
            // Fallback if no instruction info available
            reasoning.push('No instruction info found - using default N=4');
        }

        if (metrics.bottleneck) {
            reasoning.push(`Bottleneck: ${metrics.bottleneck}`);
        }

        return { suggestedN, reasoning, perInstruction };
    }

    /**
     * Escape HTML to prevent XSS
     * @param {string} text - Raw text
     * @returns {string} HTML-safe text
     */
    function escapeHtml(text) {
        if (typeof text !== 'string') {
            return '';
        }
        if (text.length === 0) {
            return '';
        }
        // Manual escape for safety - don't rely on DOM
        return text
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    }

    /**
     * Build the Godbolt API request body
     * @param {string} source - Wrapped source code
     * @param {string} userArgs - User-specified compiler arguments
     * @param {string} archFlag - Architecture-specific flag
     * @param {string} mcaCpu - MCA CPU flag
     * @returns {Object} Request body for fetch
     */
    function buildApiRequest(source, userArgs, archFlag, mcaCpu) {
        if (typeof source !== 'string' || source.length === 0) {
            throw new Error('source code is required');
        }

        const fullArgs = `${userArgs || ''} ${archFlag || ''}`.trim();

        return {
            source: source,
            options: {
                userArguments: fullArgs,
                filters: {
                    binary: false,
                    execute: false,
                    intel: true,
                    demangle: true,
                    labels: true,
                    libraryCode: false,
                    directives: true,
                    commentOnly: true,
                    trim: false
                },
                tools: [{
                    id: 'llvm-mcatrunk',
                    args: `-timeline -bottleneck-analysis ${mcaCpu || '-mcpu=skylake'}`
                }]
            }
        };
    }

    /**
     * Extract MCA output from API response
     * @param {Object} result - Godbolt API response
     * @returns {string} MCA output text
     */
    function extractMcaOutput(result) {
        if (!result || typeof result !== 'object') {
            return '';
        }

        let mcaOutput = '';
        if (result.tools && Array.isArray(result.tools) && result.tools.length > 0) {
            const mcaTool = result.tools.find(t => t && t.id === 'llvm-mcatrunk');
            if (mcaTool) {
                if (mcaTool.stderr && Array.isArray(mcaTool.stderr)) {
                    mcaOutput = mcaTool.stderr
                        .filter(s => s && typeof s.text === 'string')
                        .map(s => s.text)
                        .join('\n');
                }
                if (mcaTool.stdout && Array.isArray(mcaTool.stdout)) {
                    const stdoutText = mcaTool.stdout
                        .filter(s => s && typeof s.text === 'string')
                        .map(s => s.text)
                        .join('\n');
                    mcaOutput += stdoutText;
                }
            }
        }
        return mcaOutput;
    }

    /**
     * Extract compiler errors from API response
     * @param {Object} result - Godbolt API response
     * @returns {{hasError: boolean, text: string}}
     */
    function extractCompilerOutput(result) {
        if (!result || typeof result !== 'object') {
            return { hasError: true, text: 'Invalid response' };
        }

        let hasError = false;
        let text = '';

        if (result.stderr && Array.isArray(result.stderr) && result.stderr.length > 0) {
            text = result.stderr
                .filter(s => s && typeof s.text === 'string')
                .map(s => s.text)
                .join('\n');
            hasError = true;
        } else if (result.stdout && Array.isArray(result.stdout) && result.stdout.length > 0) {
            text = result.stdout
                .filter(s => s && typeof s.text === 'string')
                .map(s => s.text)
                .join('\n');
        } else {
            text = 'Compilation successful (no warnings)';
        }

        return { hasError, text };
    }

    /**
     * Validate user input before analysis
     * @param {string} code - User's code
     * @param {string} arch - Architecture
     * @param {string} compiler - Compiler ID
     * @returns {{valid: boolean, error: string|null}}
     */
    function validateInput(code, arch, compiler) {
        if (typeof code !== 'string' || code.trim().length === 0) {
            return { valid: false, error: 'Please enter some code to analyze' };
        }

        if (code.length > 50000) {
            return { valid: false, error: 'Code too long (max 50,000 characters)' };
        }

        const validArchs = ['skylake', 'znver4', 'apple-m1', 'cortex-a72'];
        if (!validArchs.includes(arch)) {
            return { valid: false, error: 'Invalid architecture selected' };
        }

        const validCompilers = ['clang_trunk', 'clang1800', 'clang1700'];
        if (!validCompilers.includes(compiler)) {
            return { valid: false, error: 'Invalid compiler selected' };
        }

        return { valid: true, error: null };
    }

    // Public API
    return {
        GODBOLT_API,
        wrapCode,
        getArchFlag,
        getMcaCpu,
        parseMetrics,
        generateRecommendation,
        escapeHtml,
        buildApiRequest,
        extractMcaOutput,
        extractCompilerOutput,
        validateInput
    };
})();

// Export for Node.js testing
if (typeof module !== 'undefined' && module.exports) {
    module.exports = TunerCore;
}
