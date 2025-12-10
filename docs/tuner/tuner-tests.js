// ILP Tuner Unit Tests
// Run with: node tuner-tests.js
// Or open tuner-tests.html in a browser
// Copyright (c) 2025 Matt Vanderdorff
// SPDX-License-Identifier: BSL-1.0

(function() {
    'use strict';

    // Test framework
    let passed = 0;
    let failed = 0;

    function group(name) {
        log(`\n=== ${name} ===`);
    }

    function test(name, fn) {
        try {
            fn();
            passed++;
            log(`  ✓ ${name}`);
        } catch (e) {
            failed++;
            log(`  ✗ ${name}`);
            log(`    Error: ${e.message}`);
        }
    }

    function assert(condition, message) {
        if (!condition) throw new Error(message || 'Assertion failed');
    }

    function assertEqual(actual, expected, message) {
        if (actual !== expected) {
            throw new Error(`${message || 'Failed'}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
        }
    }

    function assertContains(str, substr) {
        if (typeof str !== 'string' || !str.includes(substr)) {
            throw new Error(`Expected "${str}" to contain "${substr}"`);
        }
    }

    function assertThrows(fn) {
        let threw = false;
        try { fn(); } catch (e) { threw = true; }
        if (!threw) throw new Error('Expected function to throw');
    }

    function log(msg) {
        if (typeof console !== 'undefined') console.log(msg);
        if (typeof document !== 'undefined') {
            const output = document.getElementById('test-output');
            if (output) output.textContent += msg + '\n';
        }
    }

    // Get TunerCore
    const T = (typeof TunerCore !== 'undefined') ? TunerCore : require('./tuner-core.js');

    // ========================================
    // wrapCode tests
    // ========================================
    group('wrapCode');

    test('returns user code unchanged (passthrough)', function() {
        const code = 'sum += data[i];';
        assertEqual(T.wrapCode(code, 'skylake'), code);
    });

    test('preserves complete user code', function() {
        const code = `#include <cstddef>
void test(int* data) {
    volatile size_t n = 1000;
    int sum = 0;
    asm volatile("# LLVM-MCA-BEGIN loop");
    for (size_t i = 0; i < n; ++i) {
        sum += data[i];
    }
    asm volatile("# LLVM-MCA-END");
}`;
        assertEqual(T.wrapCode(code, 'skylake'), code);
    });

    test('handles empty code', function() {
        assertEqual(T.wrapCode('', 'skylake'), '');
    });

    test('throws on non-string userCode', function() {
        assertThrows(function() { T.wrapCode(null, 'skylake'); });
        assertThrows(function() { T.wrapCode(123, 'skylake'); });
    });

    test('throws on non-string arch', function() {
        assertThrows(function() { T.wrapCode('sum++;', null); });
    });

    // ========================================
    // getArchFlag tests
    // ========================================
    group('getArchFlag');

    test('skylake returns -march=skylake', function() {
        assertEqual(T.getArchFlag('skylake'), '-march=skylake');
    });

    test('znver4 returns -march=znver4', function() {
        assertEqual(T.getArchFlag('znver4'), '-march=znver4');
    });

    test('apple-m1 returns -mcpu=apple-m1', function() {
        assertEqual(T.getArchFlag('apple-m1'), '-mcpu=apple-m1');
    });

    test('unknown defaults to skylake', function() {
        assertEqual(T.getArchFlag('unknown'), '-march=skylake');
        assertEqual(T.getArchFlag(null), '-march=skylake');
    });

    // ========================================
    // getMcaCpu tests
    // ========================================
    group('getMcaCpu');

    test('skylake returns -mcpu=skylake', function() {
        assertEqual(T.getMcaCpu('skylake'), '-mcpu=skylake');
    });

    test('unknown defaults to skylake', function() {
        assertEqual(T.getMcaCpu('unknown'), '-mcpu=skylake');
        assertEqual(T.getMcaCpu(null), '-mcpu=skylake');
    });

    // ========================================
    // isArmArch tests
    // ========================================
    group('isArmArch');

    test('apple-m1 is ARM', function() {
        assertEqual(T.isArmArch('apple-m1'), true);
    });

    test('cortex-a72 is ARM', function() {
        assertEqual(T.isArmArch('cortex-a72'), true);
    });

    test('skylake is not ARM', function() {
        assertEqual(T.isArmArch('skylake'), false);
    });

    test('znver4 is not ARM', function() {
        assertEqual(T.isArmArch('znver4'), false);
    });

    // ========================================
    // getDefaultCompiler tests
    // ========================================
    group('getDefaultCompiler');

    test('skylake defaults to clang_trunk', function() {
        assertEqual(T.getDefaultCompiler('skylake'), 'clang_trunk');
    });

    test('znver4 defaults to clang_trunk', function() {
        assertEqual(T.getDefaultCompiler('znver4'), 'clang_trunk');
    });

    test('apple-m1 defaults to llvmasarm64_trunk', function() {
        assertEqual(T.getDefaultCompiler('apple-m1'), 'llvmasarm64_trunk');
    });

    test('cortex-a72 defaults to llvmasarm64_trunk', function() {
        assertEqual(T.getDefaultCompiler('cortex-a72'), 'llvmasarm64_trunk');
    });

    // ========================================
    // getCompilersForArch tests
    // ========================================
    group('getCompilersForArch');

    test('skylake returns x86 compilers', function() {
        const compilers = T.getCompilersForArch('skylake');
        assert(Array.isArray(compilers));
        assert(compilers.length > 0);
        assert(compilers.some(c => c.id === 'clang_trunk'));
    });

    test('apple-m1 returns ARM64 compilers', function() {
        const compilers = T.getCompilersForArch('apple-m1');
        assert(Array.isArray(compilers));
        assert(compilers.length > 0);
        assert(compilers.some(c => c.id === 'llvmasarm64_trunk'));
    });

    test('cortex-a72 returns ARM64 compilers', function() {
        const compilers = T.getCompilersForArch('cortex-a72');
        assert(Array.isArray(compilers));
        assert(compilers.some(c => c.id === 'llvmasarm64_trunk'));
    });

    test('compiler objects have id and name', function() {
        const compilers = T.getCompilersForArch('skylake');
        assert(compilers[0].id !== undefined);
        assert(compilers[0].name !== undefined);
    });

    // ========================================
    // parseMetrics tests
    // ========================================
    group('parseMetrics');

    const SAMPLE_MCA = `
Iterations:        100
Instructions:      400
Total Cycles:      103
Dispatch Width:    6
IPC:               3.88
Block RThroughput: 4.5
Bottleneck: Backend pressure on SKLPort0.
`;

    test('parses RThroughput', function() {
        assertEqual(T.parseMetrics(SAMPLE_MCA).rthroughput, 4.5);
    });

    test('parses Total Cycles', function() {
        assertEqual(T.parseMetrics(SAMPLE_MCA).cycles, 103);
    });

    test('parses Bottleneck', function() {
        assertContains(T.parseMetrics(SAMPLE_MCA).bottleneck, 'Backend');
    });

    test('parses IPC', function() {
        assertEqual(T.parseMetrics(SAMPLE_MCA).ipc, 3.88);
    });

    test('handles decimal RThroughput', function() {
        assertEqual(T.parseMetrics('Block RThroughput: 3.75').rthroughput, 3.75);
    });

    test('handles integer RThroughput', function() {
        assertEqual(T.parseMetrics('Block RThroughput: 8').rthroughput, 8.0);
    });

    test('returns empty object for empty input', function() {
        const m = T.parseMetrics('');
        assert(m.rthroughput === undefined);
    });

    test('returns empty object for non-string', function() {
        const m = T.parseMetrics(null);
        assert(m.rthroughput === undefined);
    });

    test('handles malformed RThroughput', function() {
        const m = T.parseMetrics('Block RThroughput: abc');
        assert(m.rthroughput === undefined);
    });

    // ========================================
    // parseMetrics - instructions array tests
    // ========================================
    group('parseMetrics - instructions');

    const SAMPLE_WITH_LATENCY = `
Instruction Info:
[1]: #uOps
[2]: Latency
[3]: RThroughput

[1]    [2]    [3]    Instructions:
 1      4     1.00   vaddss	%xmm0, %xmm1, %xmm2
 1      4     0.50   vmulss	%xmm2, %xmm3, %xmm4
 1      1     0.33   addl	$1, %eax

Block RThroughput: 1.0
`;

    test('parses instructions array from Instruction Info', function() {
        const m = T.parseMetrics(SAMPLE_WITH_LATENCY);
        assert(Array.isArray(m.instructions));
        assertEqual(m.instructions.length, 3);
    });

    test('extracts latency and rthroughput per instruction', function() {
        const m = T.parseMetrics(SAMPLE_WITH_LATENCY);
        assertEqual(m.instructions[0].latency, 4);
        assertEqual(m.instructions[0].rthroughput, 1.0);
        assertEqual(m.instructions[1].latency, 4);
        assertEqual(m.instructions[1].rthroughput, 0.5);
        assertEqual(m.instructions[2].latency, 1);
        assertEqual(m.instructions[2].rthroughput, 0.33);
    });

    test('extracts instruction mnemonic', function() {
        const m = T.parseMetrics(SAMPLE_WITH_LATENCY);
        assertContains(m.instructions[0].instruction, 'vaddss');
        assertContains(m.instructions[1].instruction, 'vmulss');
    });

    test('parses RThroughput alongside instructions', function() {
        const m = T.parseMetrics(SAMPLE_WITH_LATENCY);
        assertEqual(m.rthroughput, 1.0);
    });

    test('handles single instruction', function() {
        const mca = `
Instruction Info:
[1]: #uOps
[2]: Latency
[3]: RThroughput

[1]    [2]    [3]    Instructions:
 1      3     0.50   vaddps	%xmm0, %xmm1, %xmm2

Block RThroughput: 0.5
`;
        const m = T.parseMetrics(mca);
        assertEqual(m.instructions.length, 1);
        assertEqual(m.instructions[0].latency, 3);
        assertEqual(m.instructions[0].rthroughput, 0.5);
    });

    test('handles high latency instructions', function() {
        const mca = `
Instruction Info:
[1]: #uOps
[2]: Latency
[3]: RThroughput

[1]    [2]    [3]    Instructions:
 1      4     1.00   vaddss	%xmm0, %xmm1, %xmm2
 1      11    5.00   vdivss	%xmm2, %xmm3, %xmm4

Block RThroughput: 5.0
`;
        const m = T.parseMetrics(mca);
        assertEqual(m.instructions.length, 2);
        assertEqual(m.instructions[1].latency, 11);
        assertEqual(m.instructions[1].rthroughput, 5.0);
    });

    test('no instructions when Instruction Info missing', function() {
        const m = T.parseMetrics('Block RThroughput: 1.0');
        assert(m.instructions === undefined);
    });

    // ========================================
    // generateRecommendation tests (per-instruction formula)
    // ========================================
    group('generateRecommendation');

    // Helper to create instruction metrics
    function makeInstr(lat, rt) {
        return { latency: lat, rthroughput: rt, instruction: 'test' };
    }

    // Single instruction tests
    test('single instruction: N = ceil(4 / 1.0) = 4', function() {
        const rec = T.generateRecommendation({ instructions: [makeInstr(4, 1.0)] });
        assertEqual(rec.suggestedN, 4);
    });

    test('single instruction: N = ceil(4 / 0.5) = 8', function() {
        const rec = T.generateRecommendation({ instructions: [makeInstr(4, 0.5)] });
        assertEqual(rec.suggestedN, 8);
    });

    test('single instruction: N = ceil(1 / 0.33) = 4', function() {
        const rec = T.generateRecommendation({ instructions: [makeInstr(1, 0.33)] });
        assertEqual(rec.suggestedN, 4); // ceil(3.03) = 4
    });

    test('single instruction: N = ceil(11 / 5.0) = 3 (div)', function() {
        const rec = T.generateRecommendation({ instructions: [makeInstr(11, 5.0)] });
        assertEqual(rec.suggestedN, 3); // ceil(2.2) = 3
    });

    // Mixed instruction tests - key correctness test
    test('mixed: takes max of per-instruction N (add+div)', function() {
        // vaddps: L=4, RT=0.5 → N=8
        // vdivps: L=11, RT=5.0 → N=3
        // Should pick max = 8
        const rec = T.generateRecommendation({
            instructions: [makeInstr(4, 0.5), makeInstr(11, 5.0)]
        });
        assertEqual(rec.suggestedN, 8);
    });

    test('mixed: three instructions with different N values', function() {
        // N=4, N=8, N=3 → max = 8
        const rec = T.generateRecommendation({
            instructions: [makeInstr(4, 1.0), makeInstr(4, 0.5), makeInstr(11, 5.0)]
        });
        assertEqual(rec.suggestedN, 8);
    });

    test('mixed: integer add (low N) should not limit FP add (high N)', function() {
        // int add: L=1, RT=0.33 → N=4
        // FP add: L=4, RT=0.5 → N=8
        // Should be 8, not 4
        const rec = T.generateRecommendation({
            instructions: [makeInstr(1, 0.33), makeInstr(4, 0.5)]
        });
        assertEqual(rec.suggestedN, 8);
    });

    // Clamping tests
    test('N clamped to max 16', function() {
        const rec = T.generateRecommendation({ instructions: [makeInstr(10, 0.1)] });
        assertEqual(rec.suggestedN, 16); // ceil(100) clamped to 16
    });

    test('N clamped to min 2', function() {
        const rec = T.generateRecommendation({ instructions: [makeInstr(1, 5.0)] });
        assertEqual(rec.suggestedN, 2); // ceil(0.2) = 1, clamped to 2
    });

    // perInstruction output tests
    test('returns perInstruction breakdown', function() {
        const rec = T.generateRecommendation({
            instructions: [makeInstr(4, 0.5), makeInstr(11, 5.0)]
        });
        assert(Array.isArray(rec.perInstruction));
        assertEqual(rec.perInstruction.length, 2);
        assertEqual(rec.perInstruction[0].n, 8);
        assertEqual(rec.perInstruction[1].n, 3);
    });

    // Bottleneck inclusion
    test('includes bottleneck in reasoning', function() {
        const rec = T.generateRecommendation({
            instructions: [makeInstr(4, 0.5)],
            bottleneck: 'Backend'
        });
        assert(rec.reasoning.some(r => r.includes('Backend')));
    });

    // Edge cases / fallbacks
    test('fallback when no instructions', function() {
        const rec = T.generateRecommendation({});
        assertEqual(rec.suggestedN, 4);
        assert(rec.reasoning.some(r => r.includes('No instruction info')));
    });

    test('fallback when empty instructions array', function() {
        const rec = T.generateRecommendation({ instructions: [] });
        assertEqual(rec.suggestedN, 4);
    });

    test('handles null metrics', function() {
        const rec = T.generateRecommendation(null);
        assertEqual(rec.suggestedN, 4);
        assert(rec.reasoning.some(r => r.includes('No metrics')));
    });

    test('skips instruction with zero rthroughput (avoid div by zero)', function() {
        const rec = T.generateRecommendation({
            instructions: [makeInstr(4, 0), makeInstr(4, 0.5)]
        });
        assertEqual(rec.suggestedN, 8); // ignores the zero, uses 8
    });

    test('all instructions zero rthroughput uses min N', function() {
        const rec = T.generateRecommendation({
            instructions: [makeInstr(4, 0), makeInstr(4, 0)]
        });
        assertEqual(rec.suggestedN, 2); // min clamp
    });

    // ========================================
    // escapeHtml tests
    // ========================================
    group('escapeHtml');

    test('escapes < and >', function() {
        assertEqual(T.escapeHtml('<script>'), '&lt;script&gt;');
    });

    test('escapes &', function() {
        assertEqual(T.escapeHtml('a & b'), 'a &amp; b');
    });

    test('escapes quotes', function() {
        assertEqual(T.escapeHtml('"hello"'), '&quot;hello&quot;');
        assertEqual(T.escapeHtml("it's"), "it&#039;s");
    });

    test('handles empty string', function() {
        assertEqual(T.escapeHtml(''), '');
    });

    test('handles non-string', function() {
        assertEqual(T.escapeHtml(null), '');
        assertEqual(T.escapeHtml(123), '');
    });

    test('XSS prevention', function() {
        const xss = '<img src=x onerror=alert(1)>';
        const safe = T.escapeHtml(xss);
        assert(!safe.includes('<'));
        assert(!safe.includes('>'));
    });

    // ========================================
    // buildApiRequest tests
    // ========================================
    group('buildApiRequest');

    test('builds valid request for x86', function() {
        const req = T.buildApiRequest('code', '-O3', '-march=skylake', '-mcpu=skylake', 'skylake');
        assertEqual(req.source, 'code');
        assertEqual(req.options.userArguments, '-O3 -march=skylake');
        assertEqual(req.options.tools[0].id, 'llvm-mcatrunk');
    });

    test('includes MCA args for x86', function() {
        const req = T.buildApiRequest('code', '-O3', '-march=skylake', '-mcpu=skylake', 'skylake');
        assertContains(req.options.tools[0].args, '-timeline');
        assertContains(req.options.tools[0].args, '-bottleneck-analysis');
    });

    test('no MCA tools for ARM64', function() {
        const req = T.buildApiRequest('code', '-O3', '-mcpu=apple-m1', '-mcpu=apple-m1', 'apple-m1');
        assert(req.options.tools === undefined, 'tools should not be set for ARM64');
    });

    test('uses Intel syntax for x86, not for ARM64', function() {
        const reqX86 = T.buildApiRequest('code', '-O3', '-march=skylake', '-mcpu=skylake', 'skylake');
        const reqArm = T.buildApiRequest('code', '-O3', '-mcpu=apple-m1', '-mcpu=apple-m1', 'apple-m1');
        assertEqual(reqX86.options.filters.intel, true);
        assertEqual(reqArm.options.filters.intel, false);
    });

    test('throws on empty source', function() {
        assertThrows(function() { T.buildApiRequest('', '-O3', '-march=skylake', '-mcpu=skylake', 'skylake'); });
    });

    test('throws on null source', function() {
        assertThrows(function() { T.buildApiRequest(null, '-O3', '-march=skylake', '-mcpu=skylake', 'skylake'); });
    });

    // ========================================
    // extractMcaOutput tests
    // ========================================
    group('extractMcaOutput');

    test('extracts stdout', function() {
        const result = {
            tools: [{ id: 'llvm-mcatrunk', stdout: [{ text: 'output' }], stderr: [] }]
        };
        assertContains(T.extractMcaOutput(result), 'output');
    });

    test('extracts stderr', function() {
        const result = {
            tools: [{ id: 'llvm-mcatrunk', stdout: [], stderr: [{ text: 'warning' }] }]
        };
        assertContains(T.extractMcaOutput(result), 'warning');
    });

    test('handles missing tools', function() {
        assertEqual(T.extractMcaOutput({}), '');
        assertEqual(T.extractMcaOutput(null), '');
    });

    // ========================================
    // extractCompilerOutput tests
    // ========================================
    group('extractCompilerOutput');

    test('extracts stderr as error', function() {
        const result = { stderr: [{ text: 'error' }], stdout: [] };
        const out = T.extractCompilerOutput(result);
        assertEqual(out.hasError, true);
        assertContains(out.text, 'error');
    });

    test('extracts stdout as normal', function() {
        const result = { stderr: [], stdout: [{ text: 'info' }] };
        const out = T.extractCompilerOutput(result);
        assertEqual(out.hasError, false);
    });

    test('shows success for empty', function() {
        const out = T.extractCompilerOutput({ stderr: [], stdout: [] });
        assertEqual(out.hasError, false);
        assertContains(out.text, 'successful');
    });

    // ========================================
    // validateInput tests
    // ========================================
    group('validateInput');

    test('accepts valid input', function() {
        const r = T.validateInput('sum += x;', 'skylake', 'clang_trunk');
        assertEqual(r.valid, true);
    });

    test('rejects empty code', function() {
        const r = T.validateInput('', 'skylake', 'clang_trunk');
        assertEqual(r.valid, false);
    });

    test('rejects long code', function() {
        const r = T.validateInput('x'.repeat(60000), 'skylake', 'clang_trunk');
        assertEqual(r.valid, false);
    });

    test('rejects invalid arch', function() {
        const r = T.validateInput('code', 'invalid', 'clang_trunk');
        assertEqual(r.valid, false);
    });

    test('rejects invalid compiler', function() {
        const r = T.validateInput('code', 'skylake', 'invalid');
        assertEqual(r.valid, false);
    });

    test('accepts ARM64 compiler for ARM architecture', function() {
        const r = T.validateInput('code', 'apple-m1', 'llvmasarm64_trunk');
        assertEqual(r.valid, true);
    });

    test('rejects x86 compiler for ARM architecture', function() {
        const r = T.validateInput('code', 'apple-m1', 'clang_trunk');
        assertEqual(r.valid, false);
    });

    test('rejects ARM64 compiler for x86 architecture', function() {
        const r = T.validateInput('code', 'skylake', 'llvmasarm64_trunk');
        assertEqual(r.valid, false);
    });

    // ========================================
    // Integration
    // ========================================
    group('Integration');

    test('full pipeline: single instruction', function() {
        const code = 'sum += data[i];';
        const wrapped = T.wrapCode(code, 'skylake');
        assertContains(wrapped, code);

        const mcaOutput = `
Instruction Info:
[1]: #uOps
[2]: Latency
[3]: RThroughput

[1]    [2]    [3]    Instructions:
 1      4     0.50   vaddps	%xmm0, %xmm1, %xmm2

Block RThroughput: 0.5
`;
        const metrics = T.parseMetrics(mcaOutput);
        assertEqual(metrics.rthroughput, 0.5);
        assertEqual(metrics.instructions.length, 1);
        assertEqual(metrics.instructions[0].latency, 4);
        assertEqual(metrics.instructions[0].rthroughput, 0.5);

        const rec = T.generateRecommendation(metrics);
        assertEqual(rec.suggestedN, 8); // ceil(4 / 0.5) = 8
    });

    test('full pipeline: mixed instructions (correctness test)', function() {
        // This is the key test showing the bug fix
        // Old formula: maxLatency/blockRThr = 11/5 = 3 (WRONG)
        // New formula: max(4/0.5, 11/5) = max(8, 3) = 8 (CORRECT)
        const mcaOutput = `
Instruction Info:
[1]: #uOps
[2]: Latency
[3]: RThroughput

[1]    [2]    [3]    Instructions:
 1      4     0.50   vaddps	%xmm0, %xmm1, %xmm2
 1      11    5.00   vdivps	%xmm2, %xmm3, %xmm4

Block RThroughput: 5.0
`;
        const metrics = T.parseMetrics(mcaOutput);
        assertEqual(metrics.instructions.length, 2);

        const rec = T.generateRecommendation(metrics);
        // Must be 8, not 3!
        assertEqual(rec.suggestedN, 8);
        assertEqual(rec.perInstruction[0].n, 8); // vaddps
        assertEqual(rec.perInstruction[1].n, 3); // vdivps
    });

    test('full pipeline without instruction info (fallback)', function() {
        const metrics = T.parseMetrics('Block RThroughput: 4.0');
        assertEqual(metrics.rthroughput, 4.0);
        assert(metrics.instructions === undefined);

        const rec = T.generateRecommendation(metrics);
        assertEqual(rec.suggestedN, 4); // fallback default
    });

    // ========================================
    // Summary
    // ========================================
    log('\n========================================');
    log(`Tests: ${passed + failed} | Passed: ${passed} | Failed: ${failed}`);
    log('========================================\n');

    if (typeof process !== 'undefined' && process.exit) {
        process.exit(failed > 0 ? 1 : 0);
    }

    if (typeof window !== 'undefined') {
        window.TunerTestResults = { passed, failed };
    }

})();
