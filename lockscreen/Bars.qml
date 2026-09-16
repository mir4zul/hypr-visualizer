pragma ComponentBehavior: Bound
import QtQuick
import "."

Canvas {
    id: root
    clip: true
    readonly property var frame: Levels.frame
    visible: frame.lockscreen !== false
    readonly property int count: Math.max(2, Math.min(frame.maxBars ?? 512, Math.floor(width / (frame.spacing ?? 30)))) & ~1
    readonly property real step: width / Math.max(1, count)
    onFrameChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: { Levels.users++; requestPaint(); }
    Component.onDestruction: Levels.users--

    function sampleLevel(index: int): real {
        const levels = frame.levels;
        if (!levels.length) return 0;
        const p = index * (levels.length - 1) / (count - 1);
        const left = Math.floor(p);
        return levels[left] + (levels[Math.min(left + 1, levels.length - 1)] - levels[left]) * (p - left);
    }
    function blend(a: color, b: color, t: real): color {
        return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1);
    }
    function sampleColor(index: int): color {
        if (!frame.colors.length) return "transparent";
        const p = index * (frame.colors.length - 1) / (count - 1);
        const left = Math.floor(p);
        return blend(frame.colors[left], frame.colors[Math.min(left + 1, frame.colors.length - 1)], p - left);
    }
    onPaint: {
        const ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);
        const thickness = step * frame.width;
        const style = frame.style ?? 0;
        const bass = (sampleLevel(count / 2) + sampleLevel(count / 2 - 1)) / 2;
        const glow = frame.glow ?? 0;
        const opacity = frame.opacity;
        const base = frame.baseAlpha ?? 0.22;
        ctx.lineCap = "round";
        if (style === 1) {
            const scale = Math.max(0, height * frame.height - thickness);
            let peak = 0;
            for (let i = 0; i < count; ++i) peak = Math.max(peak, sampleLevel(i));
            if (peak * scale < 1) return;
            ctx.beginPath();ctx.moveTo(0, height - sampleLevel(0) * scale);
            for (let i = 0; i < count; ++i) {
                const y = height - sampleLevel(i) * scale;
                if (!i) ctx.lineTo(0.5 * step, y);
                else {
                    const previous = height - sampleLevel(i - 1) * scale;
                    const before = height - sampleLevel(Math.max(0, i - 2)) * scale;
                    const after = height - sampleLevel(Math.min(count - 1, i + 1)) * scale;
                    ctx.bezierCurveTo((i - 0.5) * step + step / 3, previous + (y - before) / 6,
                        (i + 0.5) * step - step / 3, y - (after - previous) / 6, (i + 0.5) * step, y);
                }
            }
            ctx.lineTo(width, height - sampleLevel(count - 1) * scale);
            for (let halo = 5; halo >= 0; --halo) {
                const alpha = halo ? opacity * glow * bass * 0.025 : opacity;
                if (alpha <= 0) continue;
                const gradient = ctx.createLinearGradient(0, 0, width, 0);
                for (let i = 0; i < count; ++i) {
                    const c = sampleColor(i);
                    gradient.addColorStop(i / (count - 1), Qt.rgba(c.r, c.g, c.b, alpha));
                }
                ctx.strokeStyle = gradient;ctx.lineWidth = halo ? 3 + halo * 4 : 3;ctx.stroke();
            }
            return;
        }
        for (let i = 0; i < count; ++i) {
            const bar = sampleLevel(i) * Math.max(0, height * frame.height - thickness);
            if (bar < 1) continue;
            const c = sampleColor(i);
            const x = (i + 0.5) * step;
            let y = height - bar;
            const bottom = style === 3 ? height - thickness / 2 : height + thickness;
            const lineWidth = style === 2 ? Math.min(3, thickness) : thickness;
            if (style === 3) y = Math.min(y, bottom);
            for (let halo = 5; halo >= 0; --halo) {
                const alpha = halo ? opacity * glow * bass * 0.045 : opacity;
                if (alpha <= 0) continue;
                const gradient = ctx.createLinearGradient(0, halo ? y : y - thickness / 2, 0, height);
                gradient.addColorStop(0, Qt.rgba(c.r, c.g, c.b, alpha));
                if (!halo) gradient.addColorStop(0.65, Qt.rgba(c.r, c.g, c.b, alpha));
                gradient.addColorStop(1, Qt.rgba(c.r, c.g, c.b, alpha * base));
                ctx.strokeStyle = gradient;ctx.lineWidth = lineWidth + halo * 4;
                ctx.beginPath();ctx.moveTo(x, bottom);ctx.lineTo(x, y);ctx.stroke();
            }
        }
    }
}
