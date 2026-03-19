(function () {
    function clampBrightness(value) {
        const parsed = parseInt(value, 10);
        if (Number.isNaN(parsed)) {
            return 0;
        }
        return Math.max(0, Math.min(100, parsed));
    }

    function getDisplayPayload() {
        const sliderEl = document.getElementById('lcd_bright');
        const displayEnEl = document.getElementById('display_en');
        const backlightEl = document.getElementById('display_backlight');

        const brightness = clampBrightness(sliderEl ? sliderEl.value : 0);
        const displayEnabled = !!(displayEnEl && displayEnEl.checked);
        const backlightEnabled = !!(backlightEl && backlightEl.checked);

        return {
            brightness,
            body: {
                display: {
                    en: displayEnabled,
                    brt: brightness,
                    backlight: backlightEnabled
                }
            }
        };
    }

    async function persistBrightnessOnRelease() {
        const payload = getDisplayPayload();
        try {
            await fetch('/api/config/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload.body)
            });
        } catch (error) {
            console.warn('Errore invio luminosità al rilascio slider:', error);
        }
    }

    function initSliderReleasePersist() {
        const sliderEl = document.getElementById('lcd_bright');
        if (!sliderEl) {
            return;
        }

        let lastSentBrightness = null;

        const sendIfChanged = async function () {
            const currentBrightness = clampBrightness(sliderEl.value);
            if (currentBrightness === lastSentBrightness) {
                return;
            }
            lastSentBrightness = currentBrightness;
            await persistBrightnessOnRelease();
        };

        sliderEl.addEventListener('change', sendIfChanged);
        sliderEl.addEventListener('pointerup', sendIfChanged);
        sliderEl.addEventListener('touchend', sendIfChanged, { passive: true });
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initSliderReleasePersist);
    } else {
        initSliderReleasePersist();
    }
})();