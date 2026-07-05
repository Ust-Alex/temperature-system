/**
 * ============================================================================
 * @file controls.js
 * @brief УПРАВЛЕНИЕ (КНОПКИ, МАСШТАБ, ВИДИМОСТЬ)
 * @version 6.2
 * 
 * Содержит настройку элементов управления: кнопки масштаба, поля ввода, хендлеры
 * ============================================================================
 */

// ============================================================================
// 8. УПРАВЛЕНИЕ МАСШТАБОМ
// ============================================================================
function setupControls() {
    document.querySelectorAll('.scale-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const range = this.dataset.range;
            if (range && CONFIG.RANGES[range]) {
                state.currentRange = CONFIG.RANGES[range];
                document.querySelectorAll('.scale-btn').forEach(b => b.classList.remove('active'));
                this.classList.add('active');
                performChartUpdate();
            }
        });
    });
    
    let minTimeout, maxTimeout;
    dom.get('minTemp').addEventListener('input', function() {
        clearTimeout(minTimeout);
        minTimeout = setTimeout(() => {
            const val = parseFloat(this.value);
            if (!isNaN(val) && state.chart) {
                state.chart.options.scales.y.min = val;
                state.chart.update();
            }
        }, 300);
    });
    
    dom.get('maxTemp').addEventListener('input', function() {
        clearTimeout(maxTimeout);
        maxTimeout = setTimeout(() => {
            const val = parseFloat(this.value);
            if (!isNaN(val) && state.chart) {
                state.chart.options.scales.y.max = val;
                state.chart.update();
            }
        }, 300);
    });
}

function setupVisibilityHandler() {
    document.addEventListener('visibilitychange', () => {
        if (!document.hidden && (!state.socket || state.socket.readyState !== WebSocket.OPEN)) {
            console.log('[PAGE] Вкладка активна, переподключение...');
            connectWebSocket();
        }
    });
}

function setupCleanup() {
    window.addEventListener('beforeunload', () => {
        if (state.reconnectTimeout) clearTimeout(state.reconnectTimeout);
        if (state.chartUpdateTimer) clearTimeout(state.chartUpdateTimer);
        if (state.watchdogTimer) clearInterval(state.watchdogTimer);
        if (state.socket && state.socket.readyState === WebSocket.OPEN) state.socket.close();
    });
}