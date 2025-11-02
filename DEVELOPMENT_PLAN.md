# FluidDash Development Plan - Two-Phase Approach

## Overview

This document outlines the complete development plan for FluidDash CYD Edition, split into two manageable phases.

## Phase 1: CYD Hardware Conversion ⚡ QUICK WIN

**Goal:** Get existing code running on CYD hardware
**Time:** ~2 minutes (automated)
**Risk:** Very Low
**Task File:** `CLAUDE_CODE_TASKS.md`

### What It Does:
- ✅ Converts pin definitions for CYD
- ✅ Updates LGFX display config
- ✅ Changes SPI from VSPI to HSPI
- ✅ Adds backlight PWM control
- ✅ Updates I2C pins for RTC

### What It Doesn't Change:
- ❌ Screen drawing logic
- ❌ Configuration system
- ❌ Layout structure

### Deliverable:
Working FluidDash on CYD with all current features intact.

---

## Phase 2: Screen Modularization & JSON Config 🎨 ENHANCEMENT

**Goal:** Make screen layouts editable without recompiling
**Time:** 2-3 hours
**Risk:** Low (has fallback)
**Task File:** `CLAUDE_CODE_TASKS_PHASE2.md`

### What It Adds:
- ✅ JSON-based screen layout definitions
- ✅ SD card storage for configs
- ✅ Generic drawing functions
- ✅ Layout hot-reload capability
- ✅ Fallback to original code if JSON fails

### Architecture:

```
Current (Phase 1):
main.cpp → drawMonitorMode() → hardcoded coordinates/sizes

Future (Phase 2):
SD:/screens/monitor.json → loadScreenConfig() → drawScreenFromLayout() → display
                              ↓ (if fails)
                         drawMonitorMode() (fallback)
```

### Benefits:
- Change layouts by editing JSON files
- No recompilation needed
- Multiple layout themes possible
- Easy to customize per machine
- Community layouts shareable

---

## Execution Plan

### Step 1: Phase 1 Execution
```bash
# Add CLAUDE_CODE_TASKS.md to your project
cd your-fluiddash-project

# Run Claude Code
claude-code "Read CLAUDE_CODE_TASKS.md and execute all tasks"

# Verify
pio run
pio run -t upload
pio device monitor
```

**Expected Result:** CYD displays FluidDash screens correctly

### Step 2: Test Phase 1 Thoroughly
Before moving to Phase 2, verify:
- [ ] All 4 display modes work
- [ ] Button cycles modes correctly
- [ ] WiFi connects
- [ ] Web interface accessible
- [ ] FluidNC connection works
- [ ] Temperature readings display
- [ ] Fan control functions
- [ ] RTC shows correct time

### Step 3: Phase 2 Execution
```bash
# Add CLAUDE_CODE_TASKS_PHASE2.md to your project

# Prepare SD card:
# - Format as FAT32
# - Create /screens/ directory

# Run Claude Code
claude-code "Read CLAUDE_CODE_TASKS_PHASE2.md and execute all tasks"

# Verify
pio run
```

### Step 4: Create JSON Layouts
Either:
- **Option A:** Use example monitor.json from Phase 2 tasks
- **Option B:** Convert existing layouts to JSON manually
- **Option C:** Let Claude Code generate JSON from current draw functions

### Step 5: Test Phase 2
```bash
# Insert SD card with JSON files
pio run -t upload
pio device monitor
```

Look for:
```
SD card initialized
Loaded Monitor: 13 elements
```

---

## File Organization

### Your Project Structure (After Both Phases):

```
fluiddash/
├── src/
│   └── main.cpp                        # Modified by both phases
├── platformio.ini                       # Modified by Phase 2 (adds ArduinoJson)
├── CLAUDE_CODE_TASKS.md                # Phase 1 instructions
├── CLAUDE_CODE_TASKS_PHASE2.md         # Phase 2 instructions
├── CLAUDE.md                            # General project docs
├── FLUIDDASH_HARDWARE_REFERENCE.md     # CYD pin reference
└── FLUIDDASH_WIRING_GUIDE.md           # Wiring instructions

SD Card (/screens/):
├── monitor.json                         # Monitor mode layout
├── alignment.json                       # Alignment mode layout
├── graph.json                           # Graph mode layout
└── network.json                         # Network mode layout
```

---

## Development Workflow Comparison

### Current Workflow (Pre-Phase 2):
1. Edit main.cpp with new coordinates
2. Recompile (30-60 seconds)
3. Upload to CYD (20-30 seconds)
4. Test on hardware
5. Repeat if not perfect
**Total:** 5-10 iterations = 25-50 minutes

### Future Workflow (Post-Phase 2):
1. Edit monitor.json with new coordinates
2. Eject SD card
3. Insert SD card
4. Press reload button (or reset)
**Total:** 30 seconds per iteration

**Time Savings:** ~95% for layout tweaks!

---

## Risk Mitigation

### Phase 1 Risks: MINIMAL
- **Risk:** Wrong pins break display
- **Mitigation:** Pin mappings are well-documented in LCD Wiki
- **Fallback:** Reflash original firmware

### Phase 2 Risks: LOW
- **Risk:** JSON parsing fails
- **Mitigation:** Code falls back to original draw functions
- **Risk:** SD card not detected
- **Mitigation:** Works without SD card using defaults
- **Risk:** Corrupted JSON
- **Mitigation:** Serial monitor shows parse errors, uses fallback

**Key Safety:** Legacy draw functions remain in code as backup!

---

## When to Execute Each Phase

### Execute Phase 1 When:
✅ You're ready to start using CYD hardware
✅ You want to see immediate results
✅ You need a working baseline

### Execute Phase 2 When:
✅ Phase 1 is stable and tested
✅ You're ready to customize layouts
✅ You have 2-3 hours for implementation
✅ You want to iterate on screen design

### DON'T Execute Phase 2 If:
❌ Phase 1 isn't working yet
❌ You're happy with current layouts
❌ You don't plan to customize screens
❌ You don't have an SD card

---

## Optional: Phase 2 Variations

### Minimal Phase 2 (1 hour):
- Only implement monitor.json
- Skip other modes (use legacy functions)
- Skip reload endpoint
- Basic JSON structure only

### Full Phase 2 (3 hours):
- All 4 mode JSON files
- Reload endpoint
- Comprehensive element types
- Multiple theme support

### Extended Phase 2 (5+ hours):
- Web-based layout editor
- Visual layout designer
- Cloud layout sync
- Community layout repository

---

## Success Metrics

### Phase 1 Success:
- [ ] Code compiles without errors
- [ ] Display initializes correctly
- [ ] All 4 modes render properly
- [ ] No crashes or reboots
- [ ] Serial output looks clean

### Phase 2 Success:
- [ ] ArduinoJson compiles successfully
- [ ] SD card initializes
- [ ] At least 1 JSON layout loads
- [ ] Display renders from JSON
- [ ] Fallback works when JSON missing
- [ ] Can update layout without recompiling

---

## Estimated Timeline

| Task | Time | Cumulative |
|------|------|------------|
| Phase 1 Execution | 2 min | 2 min |
| Phase 1 Upload & Test | 3 min | 5 min |
| Phase 1 Verification | 10 min | 15 min |
| **Phase 1 DONE** | | **15 min** |
| Phase 2 Execution | 30 min | 45 min |
| Create JSON Layouts | 60 min | 105 min |
| Phase 2 Upload & Test | 5 min | 110 min |
| Phase 2 Refinement | 30 min | 140 min |
| **Phase 2 DONE** | | **~2.5 hours** |

**Total Time:** ~3 hours from zero to fully modular system

---

## Next Steps

### Right Now:
1. ✅ Download `CLAUDE_CODE_TASKS.md` from this conversation
2. ✅ Add to your FluidDash project root
3. ✅ Run: `claude-code "Execute CLAUDE_CODE_TASKS.md"`
4. ✅ Verify CYD displays correctly

### After Phase 1 Works:
1. ✅ Download `CLAUDE_CODE_TASKS_PHASE2.md`
2. ✅ Format SD card (FAT32)
3. ✅ Run: `claude-code "Execute CLAUDE_CODE_TASKS_PHASE2.md"`
4. ✅ Create JSON layout files
5. ✅ Test and iterate

### Long Term:
- Share your JSON layouts
- Create custom themes
- Build layout editor
- Contribute back to community

---

## Questions?

**Q: Can I skip Phase 2?**
A: Yes! Phase 1 gives you a fully functional FluidDash on CYD.

**Q: Can I do Phase 2 later?**
A: Absolutely! Phase 2 is completely optional and non-breaking.

**Q: What if JSON parsing fails?**
A: Code automatically falls back to original draw functions. Zero risk!

**Q: Do I need to create all 4 JSON files?**
A: No! Start with one (monitor.json), others use fallback.

**Q: Can I revert Phase 2?**
A: Yes, just comment out JSON loading code. Legacy functions still there.

---

**Ready to start?** Begin with Phase 1 - get that quick win! 🚀
