# Local operating rule

- Do not modify files, start or stop computations, deploy code, commit, push, or run commands with side effects without the user's explicit permission for that action.
- Before any permitted side-effecting command, inspect its targets and failure modes. Protect existing experiment data: never reuse an output directory without validating its contents and obtaining explicit permission.
- State exactly what will be changed or started before executing it. If safety cannot be established, stop and ask the user.
