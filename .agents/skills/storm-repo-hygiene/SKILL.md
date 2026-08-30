---
name: "storm-repo-hygiene"
description: "Обеспечивает чистоту репозиториев, предотвращает коммит секретов и тяжелых бинарников, контролирует .gitignore и ветки по умолчанию."
---
# STORM Repository Hygiene

## Правила контроля чистоты Git:
1. **Защита от утечки секретов**: Блокировка файлов `.env`, `*credential*.json`, `*secret*`, `*.pfx`, `*.key`.
2. **Защита от тяжелых файлов**: Файлы >100 МБ должны исключаться через `.gitignore` и публиковаться только в Release Assets.
3. **Синхронизация веток**: Автоматическое определение `default_branch` (`main` / `master`) перед пушем.
4. **Конфигурация автора**: `git config user.name ReiKatari` и `git config user.email 45316432+ReiKatari@users.noreply.github.com`.
