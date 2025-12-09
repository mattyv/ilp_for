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

    test('wraps code with includes and harness', function() {
        const result = T.wrapCode('sum += data[i];', 'skylake');
        assertContains(result, '#include <cstddef>');
        assertContains(result, 'void analyze_loop');
        assertContains(result, 'sum += data[i];');
        assertContains(result, 'escape(sum)');
    });

    test('preserves user code exactly', function() {
        const code = 'for (int i = 0; i < n; ++i) { sum += data[i] * 2; }';
        assertContains(T.wrapCode(code, 'skylake'), code);
    });

    test('handles empty code', function() {
        assertContains(T.wrapCode('', 'skylake'), 'void analyze_loop');
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
    // generateRecommendation tests
    // ========================================
    group('generateRecommendation');

    test('N = ceil(RThroughput) for rt=1.0', function() {
        const rec = T.generateRecommendation({ rthroughput: 1.0 });
        assertEqual(rec.suggestedN, 2); // ceil(1.0) = 1, but min is 2
    });

    test('N = ceil(RThroughput) for rt=3.5', function() {
        const rec = T.generateRecommendation({ rthroughput: 3.5 });
        assertEqual(rec.suggestedN, 4); // ceil(3.5) = 4
    });

    test('N = ceil(RThroughput) for rt=8.0', function() {
        const rec = T.generateRecommendation({ rthroughput: 8.0 });
        assertEqual(rec.suggestedN, 8);
    });

    test('N clamped to max 16', function() {
        const rec = T.generateRecommendation({ rthroughput: 25.0 });
        assertEqual(rec.suggestedN, 16);
    });

    test('N clamped to min 2', function() {
        const rec = T.generateRecommendation({ rthroughput: 0.5 });
        assertEqual(rec.suggestedN, 2);
    });

    test('includes RThroughput in reasoning', function() {
        const rec = T.generateRecommendation({ rthroughput: 4.0 });
        assert(rec.reasoning.some(r => r.includes('RThroughput')));
    });

    test('includes bottleneck in reasoning', function() {
        const rec = T.generateRecommendation({ rthroughput: 4.0, bottleneck: 'Backend' });
        assert(rec.reasoning.some(r => r.includes('Backend')));
    });

    test('warns when RThroughput missing', function() {
        const rec = T.generateRecommendation({});
        assertEqual(rec.suggestedN, 4);
        assert(rec.reasoning.some(r => r.includes('not found')));
    });

    test('handles null metrics', function() {
        const rec = T.generateRecommendation(null);
        assertEqual(rec.suggestedN, 4);
        assert(rec.reasoning.some(r => r.includes('not found')));
    });

    test('handles NaN rthroughput', function() {
        const rec = T.generateRecommendation({ rthroughput: NaN });
        assertEqual(rec.suggestedN, 4);
    });

    test('handles Infinity rthroughput', function() {
        const rec = T.generateRecommendation({ rthroughput: Infinity });
        assertEqual(rec.suggestedN, 4);
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

    test('builds valid request', function() {
        const req = T.buildApiRequest('code', '-O3', '-march=skylake', '-mcpu=skylake');
        assertEqual(req.source, 'code');
        assertEqual(req.options.userArguments, '-O3 -march=skylake');
        assertEqual(req.options.tools[0].id, 'llvm-mcatrunk');
    });

    test('includes MCA args', function() {
        const req = T.buildApiRequest('code', '-O3', '-march=skylake', '-mcpu=skylake');
        assertContains(req.options.tools[0].args, '-timeline');
        assertContains(req.options.tools[0].args, '-bottleneck-analysis');
    });

    test('throws on empty source', function() {
        assertThrows(function() { T.buildApiRequest('', '-O3', '-march=skylake', '-mcpu=skylake'); });
    });

    test('throws on null source', function() {
        assertThrows(function() { T.buildApiRequest(null, '-O3', '-march=skylake', '-mcpu=skylake'); });
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

    // ========================================
    // Integration
    // ========================================
    group('Integration');

    test('full pipeline', function() {
        const code = 'sum += data[i];';
        const wrapped = T.wrapCode(code, 'skylake');
        assertContains(wrapped, code);

        const metrics = T.parseMetrics('Block RThroughput: 4.0');
        assertEqual(metrics.rthroughput, 4.0);

        const rec = T.generateRecommendation(metrics);
        assertEqual(rec.suggestedN, 4);
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
