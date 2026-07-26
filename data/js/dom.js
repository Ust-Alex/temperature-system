/**
 * ============================================================================
 * @file dom.js
 * @brief DOM-УТИЛИТЫ
 * @version 7.1
 * 
 * Содержит объект dom для работы с DOM-элементами и обновления интерфейса
 * ============================================================================
 */

// ============================================================================
// 1. ОБЪЕКТ DOM
// ============================================================================

const dom = {
    /**
     * get — получение элемента по ID
     * @param {string} id - ID элемента
     * @returns {HTMLElement|null}
     */
    get: (id) => document.getElementById(id),
    
    /**
     * updateStatus — обновление индикатора подключения
     * @param {string} status - 'online' или 'offline'
     * @param {number} attempts - количество попыток переподключения
     */
    updateStatus(status, attempts = 0) {
        const dot = this.get('panelStatusDot');
        const text = this.get('panelStatusText');
        
        if (status === 'online') {
            dot.className = 'panel-status-dot online';
            text.className = 'panel-status-text online';
            text.textContent = 'ON';
            state.reconnectAttempts = 0;
            state.reconnectStopped = false;
        } 
        else if (status === 'offline') {
            if (state.reconnectStopped) {
                dot.className = 'panel-status-dot offline';
                text.className = 'panel-status-text offline';
                text.textContent = 'ERR';
            } else {
                const isYellow = attempts <= CONFIG.RECONNECT.MAX_ATTEMPTS;
                dot.className = `panel-status-dot ${isYellow ? 'offline-yellow' : 'offline'}`;
                text.className = `panel-status-text ${isYellow ? 'offline-yellow' : 'offline'}`;
                text.textContent = 'OFF';
            }
        }
    },
    
    /**
     * updateCard — обновление карточки температуры
     * @param {string} cardId - ID карточки (card0..card5)
     * @param {number} value - значение температуры
     * @param {number} index - индекс датчика (0..5)
     */
    updateCard(cardId, value, index) {
        const card = this.get(cardId);
        if (!card) return;
        const formatted = value.toFixed(2);
        if (card.textContent !== formatted) {
            card.textContent = formatted;
        }
        state.lastValues.temps[index] = value;
    },
    
    /**
     * updateDebugInfo — обновление нижней панели отладки
     * Также обновляет время последнего измерения в верхней строке
     */
    updateDebugInfo() {
        // ----- НИЖНЯЯ ПАНЕЛЬ -----
        this.get('debugBuffer').textContent = `${state.dataBuffer.count}/${CONFIG.MAX_POINTS}`;
        this.get('debugRange').textContent = state.currentRange;
        const buttonName = Object.keys(CONFIG.RANGES).find(k => CONFIG.RANGES[k] === state.currentRange) || '?';
        this.get('debugActive').textContent = buttonName;
        this.get('debugLastTime').textContent = state.dataBuffer.lastTime || '--:--:--';
        
        // ----- ВЕРХНЯЯ СТРОКА (время последнего измерения) -----
        const modeLastTime = this.get('modeLastTime');
        if (modeLastTime) {
            modeLastTime.textContent = state.dataBuffer.lastTime || '--:--:--';
        }
    },
    
    /**
     * updateWifiStatus — обновление панели Wi-Fi
     * @param {string} mode - 'AP' или 'STA'
     * @param {string} ip - IP-адрес
     */
    updateWifiStatus(mode, ip) {
        const modeEl = this.get('wifiModeValue');
        const ipEl = this.get('wifiIPValue');
        if (modeEl) {
            modeEl.textContent = mode || '--';
            modeEl.style.color = mode === 'AP' ? '#FFA500' : (mode === 'STA' ? '#00FF00' : '#666');
        }
        if (ipEl) {
            ipEl.textContent = ip || '--';
        }
        state.wifiStatus = { mode, ip };
    }
};