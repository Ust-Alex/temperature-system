// ============================================
// 1. КОНФИГУРАЦИЯ
// ============================================
const CONFIG = {
    MAX_POINTS: 7200,
    WS_URL: 'ws://' + window.location.hostname + ':8080',
    MEASURE_INTERVAL: 1166,              // реальный интервал данных от ESP (мс)
    CHART_UPDATE_INTERVAL: 1166,          // синхронно с данными (мс)
    FAST_UPDATE_RANGES: [120],            // только для 1-минутного масштаба
    FAST_UPDATE_INTERVAL: 583,             // ~половина для плавности на 1м (мс)
    RANGES: {
        '1ч': 7200,
        '30м': 3600,
        '15м': 1800,
        '5м': 600,
        '1м': 120
    },
    RECONNECT: {
        MAX_ATTEMPTS: 5,                   // попыток до красного индикатора
        DELAYS: [1, 2, 4, 8, 15, 30, 30, 30, 30, 30],  // задержки между попытками (сек)
        MAX_DELAY: 30,                      // максимальная задержка (сек)
        MAX_TOTAL_ATTEMPTS: 20,            // максимум попыток перед остановкой
        WATCHDOG_INTERVAL: 5000             // проверка каждые 5 секунд (мс)
    },
    WATCHDOG: {
        PING_INTERVAL: 10000,               // пинг каждые 10 секунд (мс)
        DATA_TIMEOUT: 15000                  // если нет данных 15 сек - обрыв (мс)
    },
    // ДОБАВЛЕНО: Границы для зума
    ZOOM_LIMITS: {
        MIN_TEMP: 20,
        MAX_TEMP: 90,
        MIN_RANGE: 0.1
    }
};

// ============================================
// 2. СОСТОЯНИЕ
// ============================================
const state = {
    socket: null,
    reconnectAttempts: 0,
    reconnectTimeout: null,
    watchdogTimer: null,
    pingTimer: null,
    lastDataTime: Date.now(),
    pageVisible: true,
    reconnectStopped: false,
    
    dataBuffer: {
        time: new Array(CONFIG.MAX_POINTS).fill(''),
        guild: new Array(CONFIG.MAX_POINTS).fill(null),
        wall50: new Array(CONFIG.MAX_POINTS).fill(null),
        wall75: new Array(CONFIG.MAX_POINTS).fill(null),
        wall100: new Array(CONFIG.MAX_POINTS).fill(null),
        index: 0,
        count: 0,
        lastTime: ''
    },
    
    lastValues: {
        mode: -1,
        color: -1,
        time: '',
        baseTemp: null,
        temps: [null, null, null, null]
    },
    
    currentRange: 120,
    chartUpdateTimer: null,
    pendingChartUpdate: false,
    chart: null
};

// ============================================
// 3. ЗВУКИ (ИСПРАВЛЕННАЯ ВЕРСИЯ)
// ============================================
const soundManager = {
    tormazi: new Audio('/tormazi.wav'),
    zhdati: new Audio('/zhdati.wav'),
    interval: null,
    yellowCycleState: false,
    audioEnabled: false,
    audioContext: null,
    
    init() {
        console.log('[ЗВУК] Инициализация, ожидание взаимодействия...');
        
        // Создаём AudioContext заранее
        try {
            const AudioCtor = window.AudioContext || window.webkitAudioContext;
            if (AudioCtor) {
                this.audioContext = new AudioCtor();
                console.log('[ЗВУК] AudioContext создан, состояние:', this.audioContext.state);
            }
        } catch(e) {
            console.log('[ЗВУК] AudioContext не поддерживается');
        }
        
        // Универсальная функция разблокировки
        const unlock = () => {
            console.log('[ЗВУК] Попытка разблокировки...');
            
            // 1. Разблокируем AudioContext
            if (this.audioContext && this.audioContext.state === 'suspended') {
                this.audioContext.resume().then(() => {
                    console.log('[ЗВУК] AudioContext разблокирован');
                });
            }

            // 2. ПРИНУДИТЕЛЬНО включаем флаг разрешения звука
            this.audioEnabled = true;
            console.log('[ЗВУК] Флаг audioEnabled принудительно установлен');

            // 3. Проигрываем тихий тестовый звук
            const testSound = new Audio('/tormazi.wav');
            testSound.volume = 0.01; // очень тихо
            testSound.play().then(() => {
                console.log('[ЗВУК] ✅ Тестовый звук сыграл');
                testSound.pause();
                testSound.currentTime = 0;
            }).catch(e => {
                console.log('[ЗВУК] Тестовый звук не сработал:', e.message);
            });            

            // 4. Удаляем обработчики после первого раза
            document.removeEventListener('touchstart', unlock);
            document.removeEventListener('touchend', unlock);
            document.removeEventListener('click', unlock);
            document.removeEventListener('keydown', unlock);
        };
        
        // Вешаем на все возможные события
        document.addEventListener('touchstart', unlock, { once: true });
        document.addEventListener('touchend', unlock, { once: true });
        document.addEventListener('click', unlock, { once: true });
        document.addEventListener('keydown', unlock, { once: true });
    },
    
    play(sound) {
        if (!sound) return;
        
        if (!this.audioEnabled) {
            console.log('[ЗВУК] Звук заблокирован, ждём взаимодействия');
            return;
        }
        
        sound.currentTime = 0;
        sound.play().catch(e => {
            console.log('[ЗВУК] Ошибка воспроизведения:', e.message);
            // Если ошибка из-за блокировки, пробуем переразрешить
            if (e.name === 'NotAllowedError') {
                this.audioEnabled = false;
                this.init(); // перезапускаем ожидание
            }
        });
    },
    
    stopAll() {
        if (this.interval) {
            clearInterval(this.interval);
            this.interval = null;
        }
        this.yellowCycleState = false;
        console.log('[ЗВУК] Все циклы остановлены');
    },
    
    startYellowCycle() {
        this.stopAll();
        console.log('[ЗВУК] Запуск жёлтого цикла');
        this.yellowCycleState = false;
        
        this.interval = setInterval(() => {
            this.play(this.yellowCycleState ? this.tormazi : this.zhdati);
            console.log(`[ЗВУК] Жёлтый цикл: ${this.yellowCycleState ? 'tormazi' : 'zhdati'}`);
            this.yellowCycleState = !this.yellowCycleState;
        }, 60000);
    },
    
    startRedCycle() {
        this.stopAll();
        console.log('[ЗВУК] Запуск красного цикла');
        
        this.interval = setInterval(() => {
            this.play(this.tormazi);
            console.log('[ЗВУК] Красный цикл: tormazi');
        }, 30000);
    }
};

// ============================================
// 4. УТИЛИТЫ ДЛЯ РАБОТЫ С DOM
// ============================================
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
        
        const buttonName = Object.keys(CONFIG.RANGES).find(
            key => CONFIG.RANGES[key] === state.currentRange
        ) || '?';
        this.get('debugActive').textContent = buttonName;
        this.get('debugLastTime').textContent = state.dataBuffer.lastTime || '--:--:--';
    }
};

// ============================================
// 5. ГРАФИК С ЗУМОМ ПО Y (ОБНОВЛЕНО)
// ============================================
function initChart() {
    const ctx = dom.get('tempChart').getContext('2d');
    state.chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                { 
                    label: 'Гильза', 
                    data: [], 
                    borderColor: '#00FF00', 
                    borderWidth: 1.5,
                    tension: 0.3, 
                    pointRadius: 0,
                    order: 1
                },
                { 
                    label: '50см', 
                    data: [], 
                    borderColor: '#FFFF00', 
                    borderWidth: 1.5,
                    tension: 0.3, 
                    pointRadius: 0,
                    order: 2
                },
                { 
                    label: '75см', 
                    data: [], 
                    borderColor: '#00FFFF', 
                    borderWidth: 1.5,
                    tension: 0.3, 
                    pointRadius: 0,
                    order: 2
                },
                { 
                    label: '100см', 
                    data: [], 
                    borderColor: '#FFA500', 
                    borderWidth: 1.5,
                    tension: 0.3, 
                    pointRadius: 0,
                    order: 2
                },
                { 
                    label: 'База', 
                    data: [], 
                    borderColor: '#FF4500', 
                    backgroundColor: '#FF4500',
                    borderWidth: 1,
                    tension: 0,
                    pointRadius: function(context) {
                        const index = context.dataIndex;
                        return index % 8 === 0 ? 1 : 0;
                    },
                    pointHoverRadius: 4,
                    pointStyle: 'circle',
                    showLine: false,
                    order: 0
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 100 },
            spanGaps: true,
            elements: {
                line: {
                    tension: 0.3
                }
            },
            scales: {
                y: { 
                    title: { display: false },
                    min: 20,
                    max: 90,
                    grid: { color: '#333' },
                    ticks: { 
                        color: '#ccc',
                        stepSize: 0.5,
                        callback: v => v.toFixed(1)
                    }
                },
                x: { 
                    ticks: { 
                        color: '#ccc', 
                        maxTicksLimit: 8,
                        callback: function(val) {
                            const label = this.getLabelForValue(val);
                            return label || '';
                        }
                    }
                }
            },
            plugins: { 
                legend: { display: false },
                zoom: {
                    pan: {
                        enabled: true,
                        mode: 'y',
                        threshold: 5,
                        onPan: function() {
                            if (state.chart) {
                                const yScale = state.chart.scales.y;
                                dom.get('minTemp').value = yScale.min.toFixed(1);
                                dom.get('maxTemp').value = yScale.max.toFixed(1);
                            }
                        }
                    },
                    zoom: {
                        enabled: true,
                        mode: 'y',
                        pinch: { enabled: true },
                        wheel: { enabled: false },
                        onZoom: function() {
                            if (state.chart) {
                                const yScale = state.chart.scales.y;
                                dom.get('minTemp').value = yScale.min.toFixed(1);
                                dom.get('maxTemp').value = yScale.max.toFixed(1);
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

// ============================================
// 6. ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ГРАФИКА
// ============================================
function getUpdateInterval() {
    if (CONFIG.FAST_UPDATE_RANGES.includes(state.currentRange)) {
        return CONFIG.FAST_UPDATE_INTERVAL;
    }
    return CONFIG.CHART_UPDATE_INTERVAL;
}

function scheduleChartUpdate() {
    if (state.pendingChartUpdate) return;
    state.pendingChartUpdate = true;
    
    if (state.chartUpdateTimer) {
        clearTimeout(state.chartUpdateTimer);
    }
    
    state.chartUpdateTimer = setTimeout(() => {
        performChartUpdate();
        state.pendingChartUpdate = false;
        state.chartUpdateTimer = null;
    }, getUpdateInterval());
}

// ============================================
// 7. ОПТИМИЗИРОВАННАЯ ФУНКЦИЯ ОБНОВЛЕНИЯ ГРАФИКА
// ============================================
function performChartUpdate() {
    const desiredPoints = state.currentRange;
    const totalPoints = state.dataBuffer.count;
    
    // Убеждаемся, что массивы данных существуют и нужной длины
    if (!state.chart.data.labels || state.chart.data.labels.length !== desiredPoints) {
        state.chart.data.labels = new Array(desiredPoints).fill('');
        // Также сбрасываем данные датасетов при изменении размера
        state.chart.data.datasets.forEach(ds => ds.data = new Array(desiredPoints).fill(null));
    }
    
    const baseTemp = state.lastValues.baseTemp;
    const availableData = Math.min(totalPoints, desiredPoints);
    
    // Обновляем только последние availableData точек (справа)
    for (let i = 0; i < availableData; i++) {
        const dataIdx = (state.dataBuffer.index - availableData + i + CONFIG.MAX_POINTS) % CONFIG.MAX_POINTS;
        const chartIdx = desiredPoints - availableData + i;
        
        // Обновляем метку времени
        if (state.dataBuffer.time[dataIdx]) {
            state.chart.data.labels[chartIdx] = state.dataBuffer.time[dataIdx];
        }
        
        // Обновляем данные всех датасетов
        state.chart.data.datasets[0].data[chartIdx] = state.dataBuffer.guild[dataIdx];
        state.chart.data.datasets[1].data[chartIdx] = state.dataBuffer.wall50[dataIdx];
        state.chart.data.datasets[2].data[chartIdx] = state.dataBuffer.wall75[dataIdx];
        state.chart.data.datasets[3].data[chartIdx] = state.dataBuffer.wall100[dataIdx];
        state.chart.data.datasets[4].data[chartIdx] = (state.lastValues.mode === 1 && baseTemp !== null) ? baseTemp : null;
    }
    
    // Очищаем "левую" часть, если она стала пустой (например, при смене масштаба)
    for (let i = 0; i < desiredPoints - availableData; i++) {
        state.chart.data.labels[i] = '';
        state.chart.data.datasets[0].data[i] = null;
        state.chart.data.datasets[1].data[i] = null;
        state.chart.data.datasets[2].data[i] = null;
        state.chart.data.datasets[3].data[i] = null;
        state.chart.data.datasets[4].data[i] = null;
    }
    
    // Просим Chart.js перерисовать только измененные данные
    state.chart.update();
    
    dom.updateDebugInfo();
}

// ============================================
// 8. ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
// ============================================
function updateModeDisplay(mode, color, timeStr, baseTemp) {
    const modeDisplay = dom.get('modeDisplay');
    if (!modeDisplay) return;
    
    const last = state.lastValues;
    if (last.mode === mode && last.color === color && 
        last.time === timeStr && last.baseTemp === baseTemp) {
        return;
    }
    
    if (color !== last.color) {
        console.log(`[ЗВУК] Смена цвета: ${last.color} -> ${color}`);
        
        if (color === 1 || color === 2) {
            soundManager.play(soundManager.tormazi);
        }
        
        if (color === 1) {
            soundManager.startYellowCycle();
        } 
        else if (color === 2) {
            soundManager.startRedCycle();
        } 
        else if (color === 0) {
            soundManager.stopAll();
        }
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
    
    dom.updateCard('card0', data.guild, 0);
    dom.updateCard('card1', data.wall50, 1);
    dom.updateCard('card2', data.wall75, 2);
    dom.updateCard('card3', data.wall100, 3);
    
    updateModeDisplay(data.mode, data.color, data.time || '00:00', data.baseTemp);
    
    const now = new Date();
    const timeLabel = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}:${now.getSeconds().toString().padStart(2,'0')}`;
    
    state.dataBuffer.lastTime = timeLabel;
    const idx = state.dataBuffer.index;
    
    state.dataBuffer.time[idx] = timeLabel;
    state.dataBuffer.guild[idx] = data.guild;
    state.dataBuffer.wall50[idx] = data.wall50;
    state.dataBuffer.wall75[idx] = data.wall75;
    state.dataBuffer.wall100[idx] = data.wall100;
    
    state.dataBuffer.index = (idx + 1) % CONFIG.MAX_POINTS;
    if (state.dataBuffer.count < CONFIG.MAX_POINTS) {
        state.dataBuffer.count++;
    }
    
    scheduleChartUpdate();
}

// ============================================
// 9. WEBSOCKET С WATCHDOG
// ============================================
function startWatchdog() {
    if (state.watchdogTimer) clearInterval(state.watchdogTimer);
    
    state.watchdogTimer = setInterval(() => {
        const timeSinceLastData = Date.now() - state.lastDataTime;
        
        if (state.socket && state.socket.readyState === WebSocket.OPEN) {
            if (timeSinceLastData > CONFIG.WATCHDOG.DATA_TIMEOUT) {
                console.log(`[WATCHDOG] Нет данных ${timeSinceLastData/1000}с, перезапуск сокета`);
                state.socket.close();
            }
        }
        
        if (state.reconnectAttempts > CONFIG.RECONNECT.MAX_TOTAL_ATTEMPTS) {
            console.log('[WATCHDOG] Слишком много попыток, остановка');
            state.reconnectStopped = true;
            dom.updateStatus('offline', state.reconnectAttempts);
            if (state.reconnectTimeout) {
                clearTimeout(state.reconnectTimeout);
                state.reconnectTimeout = null;
            }
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
    
    if (state.reconnectTimeout) {
        clearTimeout(state.reconnectTimeout);
    }
    
    state.reconnectTimeout = setTimeout(() => {
        connectWebSocket();
    }, delayMs);
}

function connectWebSocket() {
    if (state.socket) {
        try {
            state.socket.onopen = null;
            state.socket.onclose = null;
            state.socket.onerror = null;
            state.socket.onmessage = null;
            if (state.socket.readyState !== WebSocket.CLOSED) {
                state.socket.close();
            }
        } catch (e) {
            console.log('[WS] Ошибка при очистке сокета:', e);
        }
    }
    
    dom.updateStatus('offline', state.reconnectAttempts + 1);
    
    try {
        state.socket = new WebSocket(CONFIG.WS_URL);
    } catch (e) {
        console.error('[WS] Ошибка создания сокета:', e);
        scheduleReconnect();
        return;
    }
    
    const connectionTimeout = setTimeout(() => {
        if (state.socket && state.socket.readyState === WebSocket.CONNECTING) {
            console.log('[WS] Таймаут подключения');
            state.socket.close();
        }
    }, 5000);
    
    state.socket.onopen = function() {
        clearTimeout(connectionTimeout);
        console.log('[WS] Соединение установлено');
        dom.updateStatus('online');
        state.reconnectAttempts = 0;
        state.reconnectStopped = false;
        state.lastDataTime = Date.now();
        
        if (state.reconnectTimeout) {
            clearTimeout(state.reconnectTimeout);
            state.reconnectTimeout = null;
        }
    };
    
    state.socket.onclose = function() {
        clearTimeout(connectionTimeout);
        console.log('[WS] Соединение закрыто');
        scheduleReconnect();
    };
    
    state.socket.onmessage = function(event) {
        state.lastDataTime = Date.now();
        try {
            processData(JSON.parse(event.data));
        } catch (e) {
            console.error('[WS] Ошибка парсинга:', e);
        }
    };
    
    state.socket.onerror = function(error) {
        console.error('[WS] Ошибка:', error);
        if (state.socket) {
            state.socket.close();
        }
    };
}

// ============================================
// 10. УПРАВЛЕНИЕ МАСШТАБОМ
// ============================================
function setupControls() {
    document.querySelectorAll('.scale-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const range = this.dataset.range;
            if (range && CONFIG.RANGES[range]) {
                console.log(`[КНОПКА] Нажата ${range}`);
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

// ============================================
// 11. ВОССТАНОВЛЕНИЕ ПОСЛЕ СВОРАЧИВАНИЯ
// ============================================
function setupVisibilityHandler() {
    document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
            state.pageVisible = false;
        } else {
            state.pageVisible = true;
            console.log('[PAGE] Вкладка активна, проверка соединения');
            if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
                connectWebSocket();
            }
        }
    });
}

// ============================================
// 12. ОЧИСТКА ПРИ ВЫХОДЕ
// ============================================
function setupCleanup() {
    window.addEventListener('beforeunload', () => {
        if (state.reconnectTimeout) clearTimeout(state.reconnectTimeout);
        if (state.chartUpdateTimer) clearTimeout(state.chartUpdateTimer);
        if (state.watchdogTimer) clearInterval(state.watchdogTimer);
        
        if (state.socket && state.socket.readyState === WebSocket.OPEN) {
            state.socket.close();
        }
    });
}

// ============================================
// 13. ЗАПУСК
// ============================================
window.onload = function() {
    initChart();
    setupControls();
    connectWebSocket();
    dom.updateDebugInfo();
    
    soundManager.init();
    startWatchdog();
    setupVisibilityHandler();
    setupCleanup();
    
    state.lastDataTime = Date.now();
    
    console.log('[SYSTEM] Запуск завершён, версия 4.0 (с зумом по Y)');
};