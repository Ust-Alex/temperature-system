/**
 * ============================================================================
 * @file chart.js
 * @brief ГРАФИК (CHART.JS) С ДИНАМИЧЕСКИМ ШАГОМ СЕТКИ И ЗУМОМ
 * @version 7.1
 * 
 * ОСОБЕННОСТИ:
 * - Одна ось Y с автоматическим переключением шага сетки
 * - При диапазоне > 4°C: шаг 0.5°C, толщина 2
 * - При диапазоне <= 4°C: шаг 0.1°C, толщина 1
 * - ЗУМ КОЛЁСИКОМ МЫШИ (включён)
 * - Скорость зума регулируется параметром speed
 * - Панорамирование перетаскиванием
 * - Подписи всегда с одним знаком после запятой
 * ============================================================================
 */

// ============================================================================
// 1. ИНИЦИАЛИЗАЦИЯ ГРАФИКА
// ============================================================================

function initChart() {
    const ctx = dom.get('tempChart').getContext('2d');
    
    // ----- НАБОРЫ ДАННЫХ (ЛИНИИ ДАТЧИКОВ) -----
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
    
    // ----- НАБОР ДАННЫХ ДЛЯ "БАЗЫ" (РЕЖИМ 2) -----
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
    
    // ================================================================
    // 2. СОЗДАНИЕ ГРАФИКА
    // ================================================================
    state.chart = new Chart(ctx, {
        type: 'line',
        data: { labels: [], datasets: datasets },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 100 },
            spanGaps: true,
            devicePixelRatio: 1,
            
            // ================================================================
            // 3. НАСТРОЙКА ОСЕЙ
            // ================================================================
            scales: {
                // ----- ОСЬ Y С ДИНАМИЧЕСКИМ ШАГОМ -----
                y: {
                    min: CONFIG.ZOOM_LIMITS.MIN_TEMP,
                    max: CONFIG.ZOOM_LIMITS.MAX_TEMP,
                    
                    // НАСТРОЙКА СЕТКИ
                    grid: {
                        color: '#444',
                        // Толщина будет меняться динамически
                    },
                    
                    // НАСТРОЙКА ПОДПИСЕЙ
                    ticks: {
                        color: '#ccc',
                        
                        // ====================================================
                        // ДИНАМИЧЕСКИЙ ШАГ: 0.5°C или 0.1°C
                        // ====================================================
                        stepSize: function(context) {
                            // Получаем текущий диапазон оси Y
                            const chart = context.chart;
                            const yScale = chart.scales.y;
                            if (!yScale) return 0.5;  // Защита от ошибок
                            
                            const range = yScale.max - yScale.min;
                            
                            // Если диапазон <= 4°C — шаг 0.1°C
                            if (range <= 4.0) {
                                return 0.1;
                            }
                            // Иначе шаг 0.5°C
                            return 0.5;
                        },
                        
                        // Формат подписей: один знак после запятой
                        callback: function(value) {
                            return value.toFixed(1);
                        }
                    },
                    
                    // ====================================================
                    // ДИНАМИЧЕСКАЯ ТОЛЩИНА ЛИНИЙ СЕТКИ
                    // ====================================================
                    afterUpdate: function(axis) {
                        // Вызывается после каждого обновления графика
                        const range = axis.max - axis.min;
                        // Если диапазон <= 4°C — линии тоньше (1)
                        // Иначе — толще (2)
                        axis.options.grid.lineWidth = (range <= 4.0) ? 1 : 2;
                    }
                },
                
                // ----- ОСЬ X (ВРЕМЯ) -----
                x: {
                    ticks: { 
                        color: '#ccc',
                        maxTicksLimit: 8,
                    }
                }
            },
            
            // ================================================================
            // 4. ПЛАГИНЫ
            // ================================================================
            plugins: {
                legend: { display: false },
                
                // ================================================================
                // 4a. ПЛАГИН ZOOM (МАСШТАБИРОВАНИЕ)
                // ================================================================
                zoom: {
                    // ----- ПАНОРАМИРОВАНИЕ (перетаскивание) -----
                    pan: {
                        enabled: true,                // Включено
                        mode: 'y',                    // Только по вертикали
                        threshold: 5,                 // Чувствительность (5px)
                        onPan: () => {
                            if (state.chart) {
                                const y = state.chart.scales.y;
                                dom.get('minTemp').value = y.min.toFixed(1);
                                dom.get('maxTemp').value = y.max.toFixed(1);
                            }
                        }
                    },
                    
                    // ----- ЗУМ (приближение/отдаление) -----
                    zoom: {
                        // ====================================================
                        // ЗУМ КОЛЁСИКОМ МЫШИ — ВКЛЮЧЁН!
                        // ====================================================
                        wheel: {
                            enabled: true,            // 👈 ВКЛЮЧАЕМ ЗУМ КОЛЁСИКОМ
                            speed: 0.05,              // 👈 СКОРОСТЬ ЗУМА (0.05 = медленно, 0.1 = стандарт, 0.2 = быстро)
                        },
                        
                        // ЗУМ ПАЛЬЦАМИ (на сенсорных экранах)
                        pinch: {
                            enabled: true,            // Включено
                            speed: 0.05,              // Та же скорость для сенсора
                        },
                        
                        mode: 'y',                    // Только по вертикали
                        
                        // Обработчик события зума
                        onZoom: () => {
                            if (state.chart) {
                                const y = state.chart.scales.y;
                                // Обновляем поля ввода "от" и "до"
                                dom.get('minTemp').value = y.min.toFixed(1);
                                dom.get('maxTemp').value = y.max.toFixed(1);
                            }
                        }
                    },
                    
                    // ----- ОГРАНИЧЕНИЯ ДЛЯ ЗУМА -----
                    limits: {
                        y: {
                            min: CONFIG.ZOOM_LIMITS.MIN_TEMP,     // Минимальное значение на оси Y
                            max: CONFIG.ZOOM_LIMITS.MAX_TEMP,     // Максимальное значение на оси Y
                            minRange: CONFIG.ZOOM_LIMITS.MIN_RANGE // Минимальный диапазон (0.1°C)
                        }
                    }
                }
            }
        }
    });
}

// ============================================================================
// 5. ОБНОВЛЕНИЕ ДАННЫХ ГРАФИКА
// ============================================================================

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

// ============================================================================
// 6. ЗАПИСЬ ДАННЫХ В БУФЕР
// ============================================================================

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