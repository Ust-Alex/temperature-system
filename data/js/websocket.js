/**
 * ============================================================================
 * @file websocket.js
 * @brief WEBSOCKET И WATCHDOG
 * @version 6.2
 * 
 * Содержит подключение, переподключение, watchdog и обработку сообщений
 * ============================================================================
 */

// ============================================================================
// 7. WEBSOCKET И WATCHDOG
// ============================================================================
function startWatchdog() {
    if (state.watchdogTimer) clearInterval(state.watchdogTimer);
    state.watchdogTimer = setInterval(() => {
        const timeSinceLastData = Date.now() - state.lastDataTime;
        if (state.socket && state.socket.readyState === WebSocket.OPEN) {
            if (timeSinceLastData > CONFIG.WATCHDOG.DATA_TIMEOUT) {
                console.log(`[WATCHDOG] Нет данных ${timeSinceLastData/1000}с`);
                state.socket.close();
            }
        }
        if (state.reconnectAttempts > CONFIG.RECONNECT.MAX_TOTAL_ATTEMPTS) {
            state.reconnectStopped = true;
            dom.updateStatus('offline', state.reconnectAttempts);
            if (state.reconnectTimeout) clearTimeout(state.reconnectTimeout);
        }
    }, CONFIG.RECONNECT.WATCHDOG_INTERVAL);
}

function scheduleReconnect() {
    if (state.reconnectStopped) return;
    state.reconnectAttempts++;
    dom.updateStatus('offline', state.reconnectAttempts);
    
    // Расчёт задержки: 2, 4, 8, 16, 30, 30... секунд
    const delays = [2, 4, 8, 16, 30, 30, 30, 30, 30, 30];
    const index = Math.min(state.reconnectAttempts - 1, delays.length - 1);
    const baseDelay = delays[index];
    const jitter = (Math.random() * 2) - 1;
    const delayMs = Math.max(1000, (baseDelay + jitter) * 1000);
    
    console.log(`[WS] Попытка ${state.reconnectAttempts}, через ${Math.round(delayMs/1000)}с`);
    if (state.reconnectTimeout) clearTimeout(state.reconnectTimeout);
    state.reconnectTimeout = setTimeout(() => connectWebSocket(), delayMs);
}

function connectWebSocket() {
    if (state.socket) {
        try {
            state.socket.onopen = null;
            state.socket.onclose = null;
            state.socket.onerror = null;
            state.socket.onmessage = null;
            if (state.socket.readyState !== WebSocket.CLOSED) state.socket.close();
        } catch(e) {}
    }
    
    dom.updateStatus('offline', state.reconnectAttempts + 1);
    
    try {
        state.socket = new WebSocket(CONFIG.WS_URL);
    } catch(e) {
        console.error('[WS] Ошибка:', e);
        scheduleReconnect();
        return;
    }
    
    const connectionTimeout = setTimeout(() => {
        if (state.socket && state.socket.readyState === WebSocket.CONNECTING) {
            console.log('[WS] Таймаут');
            state.socket.close();
        }
    }, 5000);
    
    state.socket.onopen = () => {
        clearTimeout(connectionTimeout);
        console.log('[WS] Подключено');
        dom.updateStatus('online');
        state.reconnectAttempts = 0;
        state.reconnectStopped = false;
        state.lastDataTime = Date.now();
        if (state.reconnectTimeout) clearTimeout(state.reconnectTimeout);
        if (state.socket.readyState === WebSocket.OPEN) {
            state.socket.send('WIFI_STATUS');
        }
    };
    
    state.socket.onclose = (event) => {
        clearTimeout(connectionTimeout);
        console.log(`[WS] Закрыто, код: ${event.code}`);
        // Если код 1000 — нормальное закрытие (мы сами закрыли), не переподключаемся
        if (event.code === 1000) {
            console.log('[WS] Штатное закрытие');
            return;
        }
        scheduleReconnect();
    };
    
    state.socket.onmessage = (event) => {
        state.lastDataTime = Date.now();
        try {
            const data = JSON.parse(event.data);
            
            // ================================================================
            // 1. Wi-Fi СТАТУС
            // ================================================================
            if (data.mode && data.ip) {
                dom.updateWifiStatus(data.mode, data.ip);
                return;
            }
            
            // ================================================================
            // 2. ДАННЫЕ ТЕМПЕРАТУР
            // ================================================================
            if (data.temps) {
                processData(data);
                return;
            }
            
            // ================================================================
            // 3. СТАТУС (сообщения от ESP)
            // ================================================================
            if (data.status) {
                console.log('[WiFi]', data.status);
                const connectBtn = dom.get('wifiConnectBtn');
                const apBtn = dom.get('wifiAPBtn');
                if (connectBtn) { connectBtn.textContent = 'Подключить'; connectBtn.disabled = false; }
                if (apBtn) { apBtn.textContent = 'AP режим'; apBtn.disabled = false; }
                return;
            }
            
            // ================================================================
            // 4. ЗВУКОВЫЕ КОМАНДЫ
            // ================================================================
            if (data.sound) {
                console.log('[SOUND] Получена команда:', data.sound);
                switch (data.sound) {
                    case 'tormazi':
                        soundManager.play(soundManager.tormazi);
                        break;
                    case 'zhdati':
                        soundManager.play(soundManager.zhdati);
                        break;
                    case 'yellow_start':
                        soundManager.startYellowCycle();
                        break;
                    case 'red_start':
                        soundManager.startRedCycle();
                        break;
                    case 'stop_all':
                        soundManager.stopAll();
                        break;
                    default:
                        console.log('[SOUND] Неизвестная команда:', data.sound);
                }
                return;
            }
            
        } catch(e) {
            console.error('[WS] Ошибка парсинга:', e);
        }
    };
    
    state.socket.onerror = (error) => {
        console.error('[WS] Ошибка:', error);
        if (state.socket) state.socket.close();
    };
}