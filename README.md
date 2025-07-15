# File Transfer Utility

**File Transfer Utility (FTU)** is a Windows tool for batch file operations (**copy**, **move**, **delete**) based on filenames or wildcard patterns. It recursively searches folders and optionally preserves directory structure.

---

## Features

* Recursive search up to 5 levels
* Wildcard pattern matching (e.g., `*.jpg`, `data_*.csv`)
* Optional folder structure preservation
* Copy, move, or delete actions
* Error logging to `FileUtilsLogs.txt`

---

## Usage

### 1. Input List

Create a file named `FileUtilsLists.txt` in the same folder as the executable. Add one filename or pattern per line:

```
*.pdf
image_*.png
log.txt
```

### 2. Run the Utility

* Select the **From Directory** (search root)
* Select the **To Directory** (target output)
* Choose an operation: Move, Copy, or Delete
* Check **Preserve folder structure** if needed

### 3. Output

* Matching files are processed recursively
* Failures and missing files are logged in `FileUtilsLogs.txt`

---

## Notes

* Supports up to 1000 patterns
* Case-insensitive search
* Uses `PathMatchSpecW` for wildcard matching
* Automatically creates output directories if needed

---

## File Reference

* `FileUtilsLists.txt`: input patterns
* `FileUtilsLogs.txt`: output logs

---

## Dependencies

* Win32 API
* shlwapi.lib
* comctl32.lib

---

## License

MIT © 2025 Gaetano Foster
Use freely for open-source and commercial projects.
