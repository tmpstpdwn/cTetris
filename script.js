const splash = document.getElementById("splash");
const status = document.getElementById("status");

// Fade in for splash screen.
requestAnimationFrame(() => {
    splash.style.opacity = "1";
});

let started = false;

function isLandscape() {
    return window.innerWidth > window.innerHeight;
}

function updateStatus() {
    if (started)
        return;

    status.textContent = isLandscape()
        ? "Click anywhere to start"
        : "Landscape required!";
}

function startGame() {
    if (started || !isLandscape())
        return;

    started = true;

    status.textContent = "Loading...";

    window.Module = {
        canvas: document.getElementById("canvas"),

        // Fade out for splash screen.
        onRuntimeInitialized() {
            splash.style.animation = "none";

            requestAnimationFrame(() => {
                splash.style.opacity = "0";
                splash.addEventListener("transitionend", () => {
                    splash.remove();
                }, { once: true });
            });
        }

    };

    window.onerror = function(msg, src, line, col, err) {
        console.error(err || msg);
    };

    // Load the actual wasm.
    const script = document.createElement("script");
    script.src = "cTetris.js";
    document.body.appendChild(script);
}

window.addEventListener("resize", updateStatus);
splash.addEventListener("click", startGame);

updateStatus();
