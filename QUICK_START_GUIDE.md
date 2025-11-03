# Quick Start Guide: Using Claude Code to Refactor FluidDash

## Prerequisites
✅ VS Code installed with Claude Code extension
✅ PlatformIO extension installed
✅ FluidDash project open in VS Code
✅ Claude Code activated and ready

## Step 1: Start Claude Code Chat
1. Open VS Code with your FluidDash project
2. Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on Mac)
3. Type "Claude Code: Open Chat" and press Enter
4. OR click the Claude icon in the sidebar

## Step 2: Give Claude Code the Instructions

Copy and paste this exact prompt into Claude Code:

```
I need to refactor my FluidDash project. The main.cpp file is over 3000 lines and needs to be split into logical modules.

I have provided detailed refactoring instructions in CLAUDE_CODE_REFACTOR_INSTRUCTIONS.md.

Please read that file and follow the step-by-step process to:
1. Break up main.cpp into modular files as specified
2. Create the new directory structure
3. Move code sections to appropriate modules
4. Add proper headers with extern declarations
5. Test compilation after each major phase
6. Ensure all functionality is preserved

Work through each phase methodically, testing compilation between phases. 
Stop and ask me if you encounter any ambiguities or issues.

Start with Phase 1: Headers and Pin Definitions.
```

## Step 3: Let Claude Code Work

Claude Code will:
- ✅ Read your main.cpp file
- ✅ Read the refactoring instructions
- ✅ Create new files and directories
- ✅ Move code sections
- ✅ Add headers and includes
- ✅ Test compilation after each phase

You can watch as it:
1. Shows you what it's doing
2. Explains its decisions
3. Asks for confirmation on unclear points
4. Tests the build after each phase

## Step 4: Review Each Phase

After each phase, Claude Code will:
1. Show you what files were created/modified
2. Run `pio run` to test compilation
3. Report any errors or warnings
4. Wait for your approval before continuing

**Your job:** Review the changes and respond with:
- "Looks good, continue to Phase 2" (if compilation succeeds)
- "Wait, I see an issue with..." (if you spot a problem)
- "The compilation error is..." (if it failed and Claude needs context)

## Step 5: Final Testing

After all phases are complete:
1. Do a clean build: `pio run -t clean && pio run`
2. Upload to hardware: `pio run -t upload`
3. Test all features to ensure nothing broke

## Common Claude Code Commands

While working with Claude Code, you can:

### View Files
```
Show me the contents of src/display/display.h
```

### Make Changes
```
Add this function to src/sensors/temperature.cpp:
[paste code]
```

### Test Compilation
```
Please run: pio run -v
```

### See Status
```
What files have we created so far?
What phase are we on?
```

### Get Help
```
I'm seeing this error: [paste error]
What does it mean?
```

## If Something Goes Wrong

### Compilation Error
1. **Don't panic!** This is normal during refactoring
2. Copy the full error message
3. Tell Claude Code: "I got this error: [paste]"
4. Claude will analyze and fix it

### Lost Track of Progress
Tell Claude: "Can you summarize what we've done so far and what's next?"

### Need to Undo
Claude Code creates files, it doesn't have built-in undo, but:
1. Use Git if you have version control (recommended!)
2. Or manually delete the created files and restore main.cpp from backup

## Pro Tips

### Before You Start
```bash
# Create a backup!
cp src/main.cpp src/main.cpp.backup

# If using git (HIGHLY RECOMMENDED):
git add .
git commit -m "Backup before refactoring"
git branch refactor-modular
git checkout refactor-modular
```

### During Refactoring
- ✅ Keep your terminal visible to see compilation output
- ✅ Have the PlatformIO panel open
- ✅ Review each file Claude creates before saying "continue"
- ✅ Test not just compilation, but actual upload if possible

### After Each Phase
```bash
# Verify compilation
pio run -v

# If git is set up:
git add .
git commit -m "Phase X complete: [description]"
```

## Expected Timeline

- **Phase 1** (Headers & Config): ~5-10 minutes
- **Phase 2** (Display): ~10-15 minutes
- **Phase 3** (Sensors): ~10-15 minutes
- **Phase 4** (Network): ~15-20 minutes
- **Phase 5** (Storage & Utils): ~5-10 minutes
- **Phase 6** (Clean main.cpp): ~5 minutes

**Total: ~60-90 minutes** including testing between phases

## Success Criteria

✅ `pio run` completes with no errors
✅ Upload to hardware succeeds
✅ Display shows all screens
✅ All sensors read correctly
✅ WiFi connects
✅ Web interface works
✅ FluidNC communication works
✅ SD card layouts load
✅ main.cpp is under 300 lines

## Troubleshooting

### "Claude Code isn't responding"
- Check your internet connection
- Try reloading VS Code
- Check Claude Code extension is active

### "Compilation takes too long"
- PlatformIO first build is always slow
- Subsequent builds are faster (incremental)
- Be patient, ~2-3 minutes is normal for ESP32

### "I want to start over"
```bash
# Restore backup
cp src/main.cpp.backup src/main.cpp

# Delete created files
rm -rf src/config src/display src/sensors src/network src/storage src/utils

# Or if using git:
git checkout main
git branch -D refactor-modular
```

## Getting Help

If you get stuck:
1. **Ask Claude Code**: It can explain what it's doing
2. **Check the error**: Most errors are simple (missing include, typo)
3. **Ask me**: I'm here to help if Claude gets confused

## Next Steps After Refactoring

Once refactoring is complete:
1. ✅ Test thoroughly on hardware
2. ✅ Document the new structure in README
3. ✅ Create JSON layout files for remaining screens
4. ✅ Consider additional improvements:
   - Add unit tests
   - Implement better error handling
   - Add logging system
   - Create configuration web interface

Good luck! The refactored code will be much easier to maintain and extend. 🚀
