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

        // No wrapper - user provides complete code
        return userCode;
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
     * Check if architecture is ARM-based
     * @param {string} arch - Architecture identifier
     * @returns {boolean} True if ARM architecture
     */
    function isArmArch(arch) {
        return arch === 'apple-m1' || arch === 'cortex-a72';
    }

    /**
     * Get default compiler for given architecture
     * @param {string} arch - Architecture identifier
     * @returns {string} Compiler identifier
     */
    function getDefaultCompiler(arch) {
        if (isArmArch(arch)) {
            return 'llvmasarm64_trunk';
        }
        return 'clang_trunk';
    }

    /**
     * Get available compilers for given architecture
     * @param {string} arch - Architecture identifier
     * @returns {Array<{id: string, name: string}>} Available compilers
     */
    function getCompilersForArch(arch) {
        if (isArmArch(arch)) {
            return [
                { id: 'llvmasarm64_trunk', name: 'AArch64 Clang (trunk)' },
                { id: 'llvmasarm64_2110', name: 'AArch64 Clang 21.1.0' },
                { id: 'llvmasarm64_1900', name: 'AArch64 Clang 19.0.0' },
                { id: 'llvmasarm64_1801', name: 'AArch64 Clang 18.0.1' }
            ];
        }
        return [
            { id: 'clang_trunk', name: 'Clang (trunk)' },
            { id: 'clang1800', name: 'Clang 18' },
            { id: 'clang1700', name: 'Clang 17' }
        ];
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
        // Format (6 columns):
        // [1]    [2]    [3]    [4]    [5]    [6]    Instructions:
        //  3      7     1.00                  U     ret
        //  2      8     0.50    *                   vpaddd ...
        const infoMatch = mcaOutput.match(/Instruction Info:[\s\S]*?Instructions:\s*\n([\s\S]*?)(?:\n\n|\nResources:|$)/);
        if (infoMatch) {
            const lines = infoMatch[1].split('\n').filter(l => l.trim());
            metrics.instructions = [];
            for (const line of lines) {
                // Match first 3 numeric columns: uops, latency, rthroughput
                // Then extract instruction mnemonic from the rest (after optional markers like *, U)
                const numMatch = line.match(/^\s*(\d+)\s+(\d+)\s+([\d.]+)/);
                if (numMatch) {
                    const lat = parseInt(numMatch[2], 10);
                    const rt = parseFloat(numMatch[3]);
                    // Extract instruction: find first word starting with a letter after the numbers
                    const remainder = line.substring(numMatch[0].length);
                    const instrMatch = remainder.match(/([a-zA-Z_][a-zA-Z0-9_]*)/);
                    const instr = instrMatch ? instrMatch[0] : 'unknown';
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
     * @param {string} arch - Architecture identifier (for tool selection)
     * @returns {Object} Request body for fetch
     */
    function buildApiRequest(source, userArgs, archFlag, mcaCpu, arch) {
        if (typeof source !== 'string' || source.length === 0) {
            throw new Error('source code is required');
        }

        const fullArgs = `${userArgs || ''} ${archFlag || ''}`.trim();

        const options = {
            userArguments: fullArgs,
            filters: {
                binary: false,
                execute: false,
                intel: !isArmArch(arch), // Use Intel syntax for x86 only
                demangle: true,
                labels: true,
                libraryCode: false,
                directives: true,
                commentOnly: true,
                trim: false
            }
        };

        // MCA only works with x86 compilers on Godbolt
        if (!isArmArch(arch)) {
            options.tools = [{
                id: 'llvm-mcatrunk',
                args: `-timeline -bottleneck-analysis ${mcaCpu || '-mcpu=skylake'}`
            }];
        }

        return { source: source, options: options };
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

        const validCompilers = getCompilersForArch(arch).map(c => c.id);
        if (!validCompilers.includes(compiler)) {
            return { valid: false, error: 'Invalid compiler for selected architecture' };
        }

        return { valid: true, error: null };
    }

    /**
     * Get instruction set identifier for Godbolt asm-doc API
     * @param {string} arch - Architecture identifier
     * @returns {string} Instruction set for API
     */
    function getInstructionSet(arch) {
        if (isArmArch(arch)) {
            return 'aarch64';
        }
        return 'amd64';
    }

    /**
     * Fetch instruction documentation from Godbolt API
     * @param {string} arch - Architecture identifier
     * @param {string} opcode - Instruction mnemonic
     * @returns {Promise<string>} Tooltip text or empty string on failure
     */
    async function fetchInstructionDoc(arch, opcode) {
        if (typeof opcode !== 'string' || opcode.length === 0) {
            return '';
        }
        const instructionSet = getInstructionSet(arch);
        const url = `${GODBOLT_API}/asm/${instructionSet}/${encodeURIComponent(opcode.toLowerCase())}`;
        try {
            const response = await fetch(url, {
                headers: { 'Accept': 'application/json' }
            });
            if (!response.ok) {
                return '';
            }
            const data = await response.json();
            return data.tooltip || '';
        } catch {
            return '';
        }
    }

    /**
     * Default regex pattern for compute (ILP-able) instructions
     * Covers common arithmetic/FP ops for x86 and ARM
     */
    const DEFAULT_COMPUTE_PATTERN = '^(v?(add|sub|mul|div|fma|sqrt|min|max|and|or|xor)|f(add|sub|mul|div|ma|ms|nma|nms|sqrt|min|max|mla|mls)|madd|msub|[su]mul|mla|mls|imul|idiv)';

    /**
     * Check if an instruction is a compute (ILP-able) instruction
     * @param {string} opcode - Instruction mnemonic
     * @param {string} pattern - Regex pattern for compute instructions
     * @returns {boolean} True if instruction is ILP-able
     */
    function isComputeInstruction(opcode, pattern) {
        if (typeof opcode !== 'string' || opcode.length === 0) {
            return false;
        }
        const regex = new RegExp(pattern || DEFAULT_COMPUTE_PATTERN, 'i');
        return regex.test(opcode);
    }

    // Public API
    return {
        GODBOLT_API,
        wrapCode,
        getArchFlag,
        getMcaCpu,
        isArmArch,
        getDefaultCompiler,
        getCompilersForArch,
        parseMetrics,
        generateRecommendation,
        escapeHtml,
        buildApiRequest,
        extractMcaOutput,
        extractCompilerOutput,
        validateInput,
        getInstructionSet,
        fetchInstructionDoc,
        DEFAULT_COMPUTE_PATTERN,
        isComputeInstruction
    };
})();

// Export for Node.js testing
if (typeof module !== 'undefined' && module.exports) {
    module.exports = TunerCore;
}
