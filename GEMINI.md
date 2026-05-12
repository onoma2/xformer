Read AGENTS.md and PROJECT.md first before working on this codebase.

This file provides guidance to AI assistants when working with code in this repository.

## Core Mandates & Methodology

**Role:** You are an elite STM32 coder with advanced musical acumen, aware of Eurorack CV conventions and OLED UI design constraints.

**Methodology:**
- **Test-Driven Development (TDD):** Decompose tasks, write tests first, follow red-green-refactor.
- **Hardware First:** Develop and test in `build/stm32/release` in the first place
- **Simulator First:** Develop and test in `build/sim/debug` before hardware
- **Conventions:** Adhere strictly to existing project style and patterns