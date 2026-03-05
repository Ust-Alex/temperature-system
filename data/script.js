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
// 3. ЗВУКИ
// ============================================
const soundManager = {
    tormazi: new Audio('/tormazi.wav'),
    zhdati: new Audio('/zhdati.wav'),
    interval: null,
    yellowCycleState: false,
    audioEnabled: false,
    
    init() {
        // Включаем звуки при первом касании (требование браузера)
        document.addEventListener('touchstart', () => {
            this.audioEnabled = true;
            console.log('[ЗВУК] Аудио разрешено');
        }, { once: true });
        
        document.addEventListener('click', () => {
            this.audioEnabled = true;
            console.log('[ЗВУК] Аудио разрешено');
        }, { once: true });
    },
    
    play(sound) {
        if (!sound || !this.audioEnabled) return;
        sound.currentTime = 0;
        sound.play().catch(e => console.log('[ЗВУК] Ошибка воспроизведения', e));
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
        }, 60000);  // 60 секунд между звуками в жёлтом режиме
    },
    
    startRedCycle() {
        this.stopAll();
        console.log('[ЗВУК] Запуск красного цикла');
        
        this.interval = setInterval(() => {
            this.play(this.tormazi);
            console.log('[ЗВУК] Красный цикл: tormazi');
        }, 30000);  // 30 секунд между звуками в красном режиме
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
                // Если попытки исчерпаны - показываем ERR
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
// 5. ГРАФИК (с порядком отрисовки)
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
                    order: 1  // выше стенок
                },
                { 
                    label: '50см', 
                    data: [], 
                    borderColor: '#FFFF00', 
                    borderWidth: 1.5,
                    tension: 0.3, 
                    pointRadius: 0,
                    order: 2  // в самом низу
                },
                { 
                    label: '75см', 
                    data: [], 
                    borderColor: '#00FFFF', 
                    borderWidth: 1.5,
                    tension: 0.3, 
                    pointRadius: 0,
                    order: 2  // тоже внизу
                },
                { 
                    label: '100см', 
                    data: [], 
                    borderColor: '#FFA500', 
                    borderWidth: 1.5,
                    tension: 0.3, 
                    pointRadius: 0,
                    order: 2  // тоже внизу
                },
                { 
                    label: 'База', 
                    data: [], 
                    borderColor: '#FF4500', 
                    backgroundColor: '#FF4500',
                    borderWidth: 1,  // 1 пиксель - тоньше стандартного, аккуратнее выглядит
                    tension: 0,
                    pointRadius: function(context) {
                        const index = context.dataIndex;
                        return index % 8 === 0 ? 1 : 0;  // точки каждые 8 измерений, размер 1px
                    },
                    pointHoverRadius: 4,
                    pointStyle: 'circle',
                    showLine: false,
                    order: 0  // ПОВЕРХ линий графиков
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 100 },
            spanGaps: true,  // соединять пропуски в данных
            elements: {
                line: {
                    tension: 0.3  // сглаживание линий
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
                        callback: function(val, index) {
                            const label = this.getLabelForValue(val);
                            return label || '';
                        }
                    }
                }
            },
            plugins: { legend: { display: false } }
        }
    });
}

function getUpdateInterval() {
    // Для 1-минутного масштаба обновляем чаще для плавности
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

function performChartUpdate() {
    const desiredPoints = state.currentRange;
    const totalPoints = state.dataBuffer.count;
    
    // Создаём метки только один раз
    if (!state.chart.data.labels || state.chart.data.labels.length !== desiredPoints) {
        state.chart.data.labels = new Array(desiredPoints).fill('');
    }
    
    // Подготавливаем массивы данных
    const datasets = [
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null)
    ];
    
    const baseTemp = state.lastValues.baseTemp;
    const availableData = Math.min(totalPoints, desiredPoints);
    
    // Заполняем данные справа (последние измерения)
    for (let i = 0; i < availableData; i++) {
        const dataIdx = (state.dataBuffer.index - availableData + i + CONFIG.MAX_POINTS) % CONFIG.MAX_POINTS;
        const chartIdx = desiredPoints - availableData + i;
        
        if (state.dataBuffer.time[dataIdx]) {
            state.chart.data.labels[chartIdx] = state.dataBuffer.time[dataIdx];
        }
        
        datasets[0][chartIdx] = state.dataBuffer.guild[dataIdx];
        datasets[1][chartIdx] = state.dataBuffer.wall50[dataIdx];
        datasets[2][chartIdx] = state.dataBuffer.wall75[dataIdx];
        datasets[3][chartIdx] = state.dataBuffer.wall100[dataIdx];
        datasets[4][chartIdx] = (state.lastValues.mode === 1 && baseTemp !== null) ? baseTemp : null;
    }
    
    // Обновляем данные датасетов
    state.chart.data.datasets.forEach((dataset, i) => {
        dataset.data = datasets[i];
    });
    
    state.chart.update();
    dom.updateDebugInfo();
}

// ============================================
// 6. ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
// ============================================
function updateModeDisplay(mode, color, timeStr, baseTemp) {
    const modeDisplay = dom.get('modeDisplay');
    if (!modeDisplay) return;
    
    const last = state.lastValues;
    if (last.mode === mode && last.color === color && 
        last.time === timeStr && last.baseTemp === baseTemp) {
        return;
    }
    
    // Звуки при смене цвета
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
    
    // Обновление текста верхней плашки
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
    // Обновляем время последних данных (для watchdog)
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
// 7. WEBSOCKET С WATCHDOG
// ============================================
function startWatchdog() {
    if (state.watchdogTimer) clearInterval(state.watchdogTimer);
    
    state.watchdogTimer = setInterval(() => {
        // Проверка: давно ли были данные
        const timeSinceLastData = Date.now() - state.lastDataTime;
        
        if (state.socket && state.socket.readyState === WebSocket.OPEN) {
            if (timeSinceLastData > CONFIG.WATCHDOG.DATA_TIMEOUT) {
                console.log(`[WATCHDOG] Нет данных ${timeSinceLastData/1000}с, перезапуск сокета`);
                state.socket.close();
            }
        }
        
        // Проверка: не зависло ли переподключение
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
    const jitter = (Math.random() * 4) - 2;  // случайное отклонение ±2 сек
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
    // Очищаем предыдущий сокет если есть
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
    
    // Таймаут на подключение (если долго висит в CONNECTING)
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
        // Принудительно закрываем, чтобы вызвался onclose
        if (state.socket) {
            state.socket.close();
        }
    };
}

// ============================================
// 8. УПРАВЛЕНИЕ МАСШТАБОМ
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
        }, 300);  // 300 мс задержки после окончания ввода
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
// 9. ВОССТАНОВЛЕНИЕ ПОСЛЕ СВОРАЧИВАНИЯ
// ============================================
function setupVisibilityHandler() {
    document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
            state.pageVisible = false;
        } else {
            state.pageVisible = true;
            console.log('[PAGE] Вкладка активна, проверка соединения');
            // Если сокет не в порядке - переподключаемся
            if (!state.socket || state.socket.readyState !== WebSocket.OPEN) {
                connectWebSocket();
            }
        }
    });
}

// ============================================
// 10. ОЧИСТКА ПРИ ВЫХОДЕ
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
// 11. ЗАПУСК
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
    
    // Первоначальная установка последнего времени
    state.lastDataTime = Date.now();
    
    console.log('[SYSTEM] Запуск завершён, версия 3.1');
};