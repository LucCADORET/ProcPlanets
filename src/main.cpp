#include "core/Engine.h"

int main(int, char**) {
	Engine engine;
	if (!engine.onInit()) return 1;

	while (engine.isRunning()) {
		engine.onFrame();
	}

	engine.onFinish();
	return 0;
}
