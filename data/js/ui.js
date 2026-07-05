/**
 * ============================================================================
 * @file ui.js
 * @brief ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
 * @version 6.2
 * 
 * Содержит функции обновления интерфейса: режим, цвета, звуки
 * ============================================================================
 */

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