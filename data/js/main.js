/**
 * ============================================================================
 * @file main.js
 * @brief ТОЧКА ВХОДА
 * @version 6.2
 * 
 * Содержит window.onload — запуск всех модулей и основных циклов
 * ============================================================================
 */

// ============================================================================
// 10. ЗАПУСК
// ============================================================================
window.onload = () => {
    // Регистрация плагина зума (ChartZoom глобально доступен из CDN)
    Chart.register(ChartZoom);
    
    initChart();
    setupControls();
    connectWebSocket();
    dom.updateDebugInfo();
    soundManager.init();
    startWatchdog();
    setupVisibilityHandler();
    setupCleanup();
    wifi.init();
    
    state.lastDataTime = Date.now();
    
    setInterval(() => {
        if (state.pendingData) {
            commitDataToBuffer(state.pendingData);
            state.pendingData = null;
        }
        performChartUpdate();
    }, CONFIG.CHART_SYNC_INTERVAL);
    
    console.log('[SYSTEM] Запуск версии 6.2 (модульная структура)');
};