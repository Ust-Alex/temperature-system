/**
 * ============================================================================
 * @file script.js
 * @brief ВЕБ-ИНТЕРФЕЙС ДЛЯ СИСТЕМЫ КОНТРОЛЯ ТЕМПЕРАТУРЫ
 * @version 6.1 (6 ДАТЧИКОВ, УПРАВЛЕНИЕ WI-FI)
 * 
 * СТРУКТУРА:
 * 1. КОНФИГУРАЦИЯ
 * 2. СОСТОЯНИЕ
 * 3. ЗВУКОВОЕ СОПРОВОЖДЕНИЕ
 * 4. DOM УТИЛИТЫ
 * 5. ГРАФИК (CHART.JS)
 * 6. ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
 * 7. WEBSOCKET И WATCHDOG
 * 8. УПРАВЛЕНИЕ МАСШТАБОМ
 * 9. УПРАВЛЕНИЕ WI-FI (НОВОЕ)
 * 10. ЗАПУСК
 * ============================================================================
 */

// ============================================================================
// 1. КОНФИГУРАЦИЯ
// ============================================================================
const CONFIG = {
    MAX_POINTS: 7200,
    WS_URL: 'ws://' + window.location.hostname + ':8080',
    MEASURE_INTERVAL: 1166,
    CHART_SYNC_INTERVAL: 1000,
    FAST_UPDATE_INTERVAL: 500,
    RANGES: {
        '1ч': 3600,
        '30м': 1800,
        '15м': 900,
        '5м': 300,
        '1м': 60
    },
    FAST_UPDATE_RANGES: [60],
    RECONNECT: {
        MAX_ATTEMPTS: 5,
        MAX_TOTAL_ATTEMPTS: 20,
        DELAYS: [1, 2, 4, 8, 15, 30, 30, 30, 30, 30],
        WATCHDOG_INTERVAL: 5000
    },
    WATCHDOG: {
        PING_INTERVAL: 10000,
        DATA_TIMEOUT: 15000
    },
    ZOOM_LIMITS: {
        MIN_TEMP: 20,
        MAX_TEMP: 90,
        MIN_RANGE: 0.1
    },
    DATASET_NAMES: ['Выход', '100см', '75см', '50см', 'Гильза', 'Куб'],
    DATASET_COLORS: ['#FF00FF', '#FFA500', '#00FFFF', '#FFFF00', '#00FF00', '#FF4500']
};

// ============================================================================
// РЕГИСТРАЦИЯ ПЛАГИНА ЗУМА
// ============================================================================
Chart.register(ChartZoom);


// ============================================================================
// 2. СОСТОЯНИЕ
// ============================================================================
const state = {
    socket: null,
    reconnectAttempts: 0,
    reconnectTimeout: null,
    watchdogTimer: null,
    reconnectStopped: false,
    lastDataTime: Date.now(),
    pageVisible: true,
    pendingData: null,
    chartUpdateTimer: null,
    dataBuffer: {
        time: new Array(CONFIG.MAX_POINTS).fill(''),
        temps: [
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null),
            new Array(CONFIG.MAX_POINTS).fill(null)
        ],
        index: 0,
        count: 0,
        lastTime: ''
    },
    lastValues: {
        mode: -1,
        color: -1,
        time: '',
        baseTemp: null,
        temps: [null, null, null, null, null, null]
    },
    currentRange: 60,
    chart: null,
    wifiStatus: { mode: '--', ip: '--' }
};

// ============================================================================
// 3. ЗВУКОВОЕ СОПРОВОЖДЕНИЕ
// ============================================================================
const soundManager = {
    tormazi: new Audio('/tormazi.wav'),
    zhdati: new Audio('/zhdati.wav'),
    interval: null,
    yellowCycleState: false,
    audioEnabled: false,
    audioContext: null,
    
    init() {
        console.log('[ЗВУК] Инициализация...');
        try {
            const AudioCtor = window.AudioContext || window.webkitAudioContext;
            if (AudioCtor) {
                this.audioContext = new AudioCtor();
            }
        } catch(e) {}
        
        const unlock = () => {
            if (this.audioContext && this.audioContext.state === 'suspended') {
                this.audioContext.resume();
            }
            this.audioEnabled = true;
            document.removeEventListener('touchstart', unlock);
            document.removeEventListener('touchend', unlock);
            document.removeEventListener('click', unlock);
            document.removeEventListener('keydown', unlock);
        };
        
        document.addEventListener('touchstart', unlock, { once: true });
        document.addEventListener('touchend', unlock, { once: true });
        document.addEventListener('click', unlock, { once: true });
        document.addEventListener('keydown', unlock, { once: true });
    },
    
    play(sound) {
        if (!sound || !this.audioEnabled) return;
        sound.currentTime = 0;
        sound.play().catch(e => {});
    },
    
    stopAll() {
        if (this.interval) {
            clearInterval(this.interval);
            this.interval = null;
        }
        this.yellowCycleState = false;
    },
    
    startYellowCycle() {
        this.stopAll();
        this.yellowCycleState = false;
        this.interval = setInterval(() => {
            this.play(this.yellowCycleState ? this.tormazi : this.zhdati);
            this.yellowCycleState = !this.yellowCycleState;
        }, 60000);
    },
    
    startRedCycle() {
        this.stopAll();
        this.interval = setInterval(() => {
            this.play(this.tormazi);
        }, 30000);
    }
};

// ============================================================================
// 4. DOM УТИЛИТЫ
// ============================================================================
const dom = {
    get: (id) => document.getElementById(id),
    
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
    
    updateCard(cardId, value, index) {
        const card = this.get(cardId);
        if (!card) return;
        const formatted = value.toFixed(2);
        if (card.textContent !== formatted) {
            card.textContent = formatted;
        }
        state.lastValues.temps[index] = value;
    },
    
    updateDebugInfo() {
        this.get('debugBuffer').textContent = `${state.dataBuffer.count}/${CONFIG.MAX_POINTS}`;
        this.get('debugRange').textContent = state.currentRange;
        const buttonName = Object.keys(CONFIG.RANGES).find(k => CONFIG.RANGES[k] === state.currentRange) || '?';
        this.get('debugActive').textContent = buttonName;
        this.get('debugLastTime').textContent = state.dataBuffer.lastTime || '--:--:--';
    },
    
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

// ============================================================================
// 5. ГРАФИК (CHART.JS)
// ============================================================================
function initChart() {
    const ctx = dom.get('tempChart').getContext('2d');
    
    const datasets = CONFIG.DATASET_NAMES.map((name, i) => {
        const color = CONFIG.DATASET_COLORS[i];
        return {
            label: name,
            data: [],
            borderColor: color,
            borderWidth: 1.5,
            tension: 0.3,
            pointRadius: 0,
            order: i === 4 ? 1 : 2
        };
    });
    
    datasets.push({
        label: 'База',
        data: [],
        borderColor: '#FF4500',
        backgroundColor: '#FF4500',
        borderWidth: 1,
        tension: 0,
        pointRadius: (ctx) => ctx.dataIndex % 8 === 0 ? 1 : 0,
        showLine: false,
        order: 0
    });
    
    state.chart = new Chart(ctx, {
        type: 'line',
        data: { labels: [], datasets: datasets },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 100 },
            spanGaps: true,
            scales: {
                y: {
                    min: CONFIG.ZOOM_LIMITS.MIN_TEMP,
                    max: CONFIG.ZOOM_LIMITS.MAX_TEMP,
                    grid: { color: '#333' },
                    ticks: { color: '#ccc', stepSize: 0.5, callback: v => v.toFixed(1) }
                },
                x: {
                    ticks: { color: '#ccc', maxTicksLimit: 8 }
                }
            },
            plugins: {
                legend: { display: false },
                zoom: {
                    pan: {
                        enabled: true,
                        mode: 'y',
                        threshold: 5,
                        onPan: () => {
                            if (state.chart) {
                                const y = state.chart.scales.y;
                                dom.get('minTemp').value = y.min.toFixed(1);
                                dom.get('maxTemp').value = y.max.toFixed(1);
                            }
                        }
                    },
                    zoom: {
                        wheel: { enabled: false },
                        pinch: { enabled: true },
                        mode: 'y',
                        onZoom: () => {
                            if (state.chart) {
                                const y = state.chart.scales.y;
                                dom.get('minTemp').value = y.min.toFixed(1);
                                dom.get('maxTemp').value = y.max.toFixed(1);
                            }
                        }
                    },
                    limits: {
                        y: {
                            min: CONFIG.ZOOM_LIMITS.MIN_TEMP,
                            max: CONFIG.ZOOM_LIMITS.MAX_TEMP,
                            minRange: CONFIG.ZOOM_LIMITS.MIN_RANGE
                        }
                    }
                }
            }
        }
    });
}

function performChartUpdate() {
    const desiredPoints = state.currentRange;
    const totalPoints = state.dataBuffer.count;
    
    if (!state.chart.data.labels || state.chart.data.labels.length !== desiredPoints) {
        state.chart.data.labels = new Array(desiredPoints).fill('');
        state.chart.data.datasets.forEach(ds => ds.data = new Array(desiredPoints).fill(null));
    }
    
    const baseTemp = state.lastValues.baseTemp;
    const availableData = Math.min(totalPoints, desiredPoints);
    
    for (let i = 0; i < availableData; i++) {
        const dataIdx = (state.dataBuffer.index - availableData + i + CONFIG.MAX_POINTS) % CONFIG.MAX_POINTS;
        const chartIdx = desiredPoints - availableData + i;
        
        if (state.dataBuffer.time[dataIdx]) {
            state.chart.data.labels[chartIdx] = state.dataBuffer.time[dataIdx];
        }
        
        for (let s = 0; s < 6; s++) {
            state.chart.data.datasets[s].data[chartIdx] = state.dataBuffer.temps[s][dataIdx];
        }
        
        state.chart.data.datasets[6].data[chartIdx] = (state.lastValues.mode === 1 && baseTemp !== null) ? baseTemp : null;
    }
    
    for (let i = 0; i < desiredPoints - availableData; i++) {
        state.chart.data.labels[i] = '';
        state.chart.data.datasets.forEach(ds => ds.data[i] = null);
    }
    
    state.chart.update();
    dom.updateDebugInfo();
}

function commitDataToBuffer(temps) {
    if (!temps || !Array.isArray(temps) || temps.length < 6) return;
    
    const now = new Date();
    const timeLabel = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}:${now.getSeconds().toString().padStart(2,'0')}`;
    
    state.dataBuffer.lastTime = timeLabel;
    const idx = state.dataBuffer.index;
    
    state.dataBuffer.time[idx] = timeLabel;
    for (let s = 0; s < 6; s++) {
        state.dataBuffer.temps[s][idx] = temps[s];
    }
    
    state.dataBuffer.index = (idx + 1) % CONFIG.MAX_POINTS;
    if (state.dataBuffer.count < CONFIG.MAX_POINTS) state.dataBuffer.count++;
}

// ============================================================================
// 6. ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
// ============================================================================
function updateModeDisplay(mode, color, timeStr, baseTemp) {
    const modeDisplay = dom.get('modeDisplay');
    if (!modeDisplay) return;
    
    const last = state.lastValues;
    if (last.mode === mode && last.color === color && last.time === timeStr && last.baseTemp === baseTemp) return;
    
    if (color !== last.color) {
        if (color === 1 || color === 2) soundManager.play(soundManager.tormazi);
        if (color === 1) soundManager.startYellowCycle();
        else if (color === 2) soundManager.startRedCycle();
        else if (color === 0) soundManager.stopAll();
    }
    
    let newText, newClass;
    if (mode === 0) {
        newClass = 'mode-bar mode0';
        newText = 'MODE 1 (СТАБИЛИЗАЦИЯ) ' + timeStr;
    } else {
        const colorClass = color === 1 ? 'mode1-yellow' : (color === 2 ? 'mode1-red' : 'mode1-green');
        newClass = `mode-bar ${colorClass}`;
        const baseStr = baseTemp !== null ? baseTemp.toFixed(2) : '--.--';
        newText = `MODE 2 (РАБОЧИЙ ${baseStr}) ${timeStr}`;
    }
    
    modeDisplay.className = newClass;
    modeDisplay.textContent = newText;
    Object.assign(last, { mode, color, time: timeStr, baseTemp });
}

function processData(data) {
    state.lastDataTime = Date.now();
    
    if (data.temps && Array.isArray(data.temps) && data.temps.length >= 6) {
        for (let i = 0; i < 6; i++) {
            dom.updateCard(`card${i}`, data.temps[i], i);
        }
        state.pendingData = data.temps;
    }
    
    updateModeDisplay(data.mode, data.color, data.time || '00:00', data.baseTemp);
}

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
    
    const delays = CONFIG.RECONNECT.DELAYS;
    const baseDelay = delays[Math.min(state.reconnectAttempts - 1, delays.length - 1)];
    const jitter = (Math.random() * 4) - 2;
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
        // Запрашиваем статус Wi-Fi при подключении
        if (state.socket.readyState === WebSocket.OPEN) {
            state.socket.send('WIFI_STATUS');
        }
    };
    
    state.socket.onclose = () => {
        clearTimeout(connectionTimeout);
        console.log('[WS] Закрыто');
        scheduleReconnect();
    };
    
    state.socket.onmessage = (event) => {
        state.lastDataTime = Date.now();
        try {
            const data = JSON.parse(event.data);
            if (data.mode && data.ip) {
                // Это Wi-Fi статус
                dom.updateWifiStatus(data.mode, data.ip);
            } else if (data.temps) {
                processData(data);
            } else if (data.status) {
                console.log('[WiFi]', data.status);
                const connectBtn = dom.get('wifiConnectBtn');
                const apBtn = dom.get('wifiAPBtn');
                if (connectBtn) { connectBtn.textContent = 'Подключить'; connectBtn.disabled = false; }
                if (apBtn) { apBtn.textContent = 'AP режим'; apBtn.disabled = false; }
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

// ============================================================================
// 9. УПРАВЛЕНИЕ WI-FI (НОВОЕ)
// ============================================================================
const wifi = {
    init() {
        this.setupForm();
        // Запросим статус при загрузке (через WebSocket)
        setTimeout(() => {
            if (state.socket && state.socket.readyState === WebSocket.OPEN) {
                state.socket.send('WIFI_STATUS');
            }
        }, 1000);
    },
    
    setupForm() {
        const connectBtn = dom.get('wifiConnectBtn');
        const apBtn = dom.get('wifiAPBtn');
        const ssidInput = dom.get('wifiSSID');
        const passInput = dom.get('wifiPassword');
        
        if (connectBtn) {
            connectBtn.addEventListener('click', () => {
                const ssid = ssidInput.value.trim();
                const pass = passInput.value.trim();
                if (ssid.length === 0) {
                    alert('Введите SSID');
                    return;
                }
                if (state.socket && state.socket.readyState === WebSocket.OPEN) {
                    state.socket.send(`WIFI_SET:${ssid}:${pass}`);
                    connectBtn.textContent = 'Сохранение...';
                    connectBtn.disabled = true;
                }
            });
        }
        
        if (apBtn) {
            apBtn.addEventListener('click', () => {
                if (confirm('Переключиться в режим AP (точка доступа)?')) {
                    if (state.socket && state.socket.readyState === WebSocket.OPEN) {
                        state.socket.send('WIFI_AP');
                        apBtn.textContent = 'Переключение...';
                        apBtn.disabled = true;
                    }
                }
            });
        }
    }
};

// ============================================================================
// 10. ЗАПУСК
// ============================================================================
window.onload = () => {
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
    
    console.log('[SYSTEM] Запуск версии 6.1 (6 датчиков, управление Wi-Fi)');
};