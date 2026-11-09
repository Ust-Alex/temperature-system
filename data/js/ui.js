/**
 * ============================================================================
 * @file ui.js
 * @brief ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
 * @version 7.2
 * 
 * ОСОБЕННОСТИ:
 * - Верхняя строка: слева — время последнего измерения,
 *   по центру — текст режима + время стабилизации (вместе, как в оригинале)
 * - Обновление карточек температур
 * - Управление звуковыми сигналами
 * ============================================================================
 */

// ============================================================================
// 1. ОБНОВЛЕНИЕ ВЕРХНЕЙ СТРОКИ (режим + время)
// ============================================================================

/**
 * updateModeDisplay — обновляет верхнюю строку интерфейса
 * @param {number} mode - 0 = режим 1 (стабилизация), 1 = режим 2 (рабочий)
 * @param {number} color - 0 = зелёный, 1 = жёлтый, 2 = красный
 * @param {string} timeStr - время стабилизации (приходит с ESP)
 * @param {number|null} baseTemp - базовая температура для режима 2
 * 
 * Верхняя строка содержит два элемента:
 * - Слева: время последнего измерения (из state.dataBuffer.lastTime)
 * - По центру: текст режима + время стабилизации (как было в оригинале)
 */
function updateModeDisplay(mode, color, timeStr, baseTemp) {
    const modeDisplay = dom.get('modeDisplay');
    if (!modeDisplay) return;
    
    const last = state.lastValues;
    if (last.mode === mode && last.color === color && last.time === timeStr && last.baseTemp === baseTemp) return;
    
    // ----- ЗВУКОВОЕ СОПРОВОЖДЕНИЕ -----
    if (color !== last.color) {
        if (color === 1 || color === 2) soundManager.play(soundManager.tormazi);
        if (color === 1) soundManager.startYellowCycle();
        else if (color === 2) soundManager.startRedCycle();
        else if (color === 0) soundManager.stopAll();
    }
    
    // ----- ПОЛУЧАЕМ ЭЛЕМЕНТЫ ДЛЯ ОБНОВЛЕНИЯ -----
    const modeText = dom.get('modeText');
    const modeLastTime = dom.get('modeLastTime');
    
    let newClass, fullText;
    const timeDisplay = timeStr || '00:00';
    
    // ----- ОПРЕДЕЛЯЕМ РЕЖИМ И ФОРМИРУЕМ ПОЛНЫЙ ТЕКСТ -----
    if (mode === 0) {
        newClass = 'mode-bar mode0';
        fullText = `MODE 1 (СТАБИЛИЗАЦИЯ) ${timeDisplay}`;
    } else {
        const colorClass = color === 1 ? 'mode1-yellow' : (color === 2 ? 'mode1-red' : 'mode1-green');
        newClass = `mode-bar ${colorClass}`;
        const baseStr = baseTemp !== null ? baseTemp.toFixed(2) : '--.--';
        fullText = `MODE 2 (РАБОЧИЙ ${baseStr}) ${timeDisplay}`;
    }
    
    // ----- ПРИМЕНЯЕМ ИЗМЕНЕНИЯ -----
    modeDisplay.className = newClass;
    
    // Обновляем текст режима + время стабилизации (вместе, по центру)
    if (modeText) {
        modeText.textContent = fullText;
    }
    
    // Обновляем время последнего измерения (слева)
    if (modeLastTime) {
        modeLastTime.textContent = state.dataBuffer.lastTime || '--:--:--';
    }
    
    // Сохраняем последние значения
    Object.assign(last, { mode, color, time: timeStr, baseTemp });
}

// ============================================================================
// 2. ОБНОВЛЕНИЕ КАРТОЧЕК ТЕМПЕРАТУР
// ============================================================================

/**
 * processData — обрабатывает входящие данные от ESP
 * @param {Object} data - объект с полями temps, mode, color, time, baseTemp
 */
function processData(data) {
    state.lastDataTime = Date.now();
    
    // Обновляем карточки температур
    if (data.temps && Array.isArray(data.temps) && data.temps.length >= 7) {
        for (let i = 0; i < 7; i++) {
            dom.updateCard(`card${i}`, data.temps[i], i);
        }
        state.pendingData = data.temps;
    }
    
    // Обновляем верхнюю строку
    updateModeDisplay(data.mode, data.color, data.time || '00:00', data.baseTemp);
}