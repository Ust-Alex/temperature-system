/**
 * ============================================================================
 * @file chart.js
 * @brief ГРАФИК (CHART.JS)
 * @version 6.2
 * 
 * Содержит инициализацию графика, обновление данных и работу с буфером
 * ============================================================================
 */

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
            devicePixelRatio: 1,
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