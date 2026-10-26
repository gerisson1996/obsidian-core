# NX Port Hub (Obsidian Core)

[Português](#português) • [English](#english)

---

## Português

O **NX Port Hub** é um ecossistema homebrew para o Nintendo Switch que permite descobrir, baixar, verificar e instalar ports de jogos (especialmente ports nativos por recompilação de Xbox clássico e PC) diretamente no console via Wi-Fi.

### Componentes

* **Catálogo Central:** Hospedado via GitHub Pages em [`catalog/ports.json`](https://gerisson1996.github.io/obsidian-core/catalog/ports.json).
* **Site da Vitrine:** Interface web pública em [`docs/index.html`](https://gerisson1996.github.io/obsidian-core/).
* **Aplicativo Switch (.nro):** Código-fonte C++ com SDL2 e libnx na pasta [`app/`](./app).
* **Automação (CI/CD):** Workflows em `.github/workflows/` para publicação do catálogo e compilação do `.nro`.

### Recursos do Aplicativo

* **Explorar Ports:** Navegação por cards com status de compatibilidade (Jogável a 60 FPS / Experimental), tamanho e autor.
* **Detalhes & Verificação:** Inspeção do cartão MicroSD para avisar ao usuário se os arquivos originais do jogo (ex: `default.xbe`, `bigfile.bin`) estão presentes ou ausentes.
* **Download Seguro:** Barra de progresso em tempo real, cálculo de velocidade e validação de hash **SHA-256** antes de instalar.
* **Preservação de Dados:** Atualiza apenas o binário `.nro`, mantendo intactos seus saves, configurações (`mods.txt`) e pastas de jogos (`game/`).
* **Proteção de Armazenamento:** Verifica o espaço livre no SD (`sdmc:/`) antes de iniciar downloads.

### Como Compilar

Requisitos: [devkitPro](https://devkitpro.org/) com `devkitA64`, `libnx` e portlibs (`switch-sdl2`, `switch-curl`).

```bash
cd app
make -j$(nproc)
```

O arquivo `nx-port-hub.nro` gerado deve ser colocado em `sdmc:/switch/nx-port-hub/nx-port-hub.nro`.

---

## English

**NX Port Hub** is a homebrew hub for Nintendo Switch designed to discover, download, verify, and install game ports (including Xbox static recompilations) directly over Wi-Fi.

### Features

* **Remote Catalog:** Live JSON catalog hosted on GitHub Pages.
* **Pre-flight Checks:** Verifies SD card space and confirms required original game files exist before launching.
* **Integrity Validation:** Validates SHA-256 checksums on all downloads before finalizing.
* **Safe Updates:** Atomic updates preserving configs, saves, and assets.

### License & Fair Use

Packages distributed through this catalog must strictly contain original or open-source homebrew binaries. Commercial game assets must be extracted by users from their own legitimate copies.
