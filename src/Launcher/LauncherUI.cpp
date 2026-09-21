#include <WiFi.h>
#include "LauncherUI.h"
#include "../Kernel/Core/HarixKernel.h"
#include "../File System/FileSystem.h"

extern int currentState;

String LauncherUI::appPaths[50];
String LauncherUI::appNames[50];
String LauncherUI::currentCategory;
LauncherItem LauncherUI::items[50]; 
String LauncherUI::appCategories[50];

int LauncherUI::appCount = 0;
int LauncherUI::totalItems = 0;
int LauncherUI::scrollOffset = 0;
int LauncherUI::selectedIndex = 1;
int LauncherUI::lastCategoryIndex = 0;

bool LauncherUI::appIsFolder[50];
bool LauncherUI::needsRescan = true;

int launcherSubMenu = 0;

void LauncherUI::requestRescan() {
    needsRescan = true;
}

void LauncherUI::scanLocalApps() {
    appCount = 0;
    
    // Obtenção dinâmica de dimensões para suporte a 240x135 e 240x320
    const int16_t screenW = tft.width();
    const int16_t screenH = tft.height();
    const bool isSmallScreen = (screenH <= 135);

    const int16_t barWidth = (screenW * 80) / 100;
    const int16_t barHeight = isSmallScreen ? 8 : 12;
    const int16_t barX = (screenW - barWidth) / 2;
    const int16_t centerY = screenH / 2;
    const int16_t barY = isSmallScreen ? (centerY + 8) : (centerY + 15);

    const char* appDirs[] = { "/local/apps/", "/sd/apps/" };
    
    for (int d = 0; d < 2; d++) {
        if (!FileSystem::exists(appDirs[d])) continue;
        
        FileEntry entries[50];
        int count = FileSystem::listDirectory(appDirs[d], entries, 50);
        
        for (int i = 0; i < count && appCount < 50; i++) {
            if (count > 0) {
                int fillWidth = map(i + 1, 0, count, 0, barWidth - 4);
                // Garante preenchimento interno exato sem estourar as bordas
                tft.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, TFT_GREEN);
            }

            if (entries[i].isDir) {
                String appJsonPath = entries[i].path;
                if (!appJsonPath.endsWith("/")) appJsonPath += "/";
                appJsonPath += "app.json";
                
                if (FileSystem::exists(appJsonPath.c_str())) {
                    String jsonContent = FileSystem::readTextFile(appJsonPath.c_str());
                    String name = FileSystem::parseJsonValue(jsonContent, "name");
                    String category = FileSystem::parseJsonValue(jsonContent, "category");
                    
                    if (category.length() == 0) category = "General";

                    if (name.length() > 0) {
                        bool duplicate = false;
                        for (int j = 0; j < appCount; j++) {
                            if (appNames[j] == name) { duplicate = true; break; }
                        }
                        if (duplicate) continue;
                        
                        appPaths[appCount] = entries[i].path;
                        appNames[appCount] = name;
                        appCategories[appCount] = category;
                        appIsFolder[appCount] = true;
                        appCount++;
                    }
                }
            } else {
                String fname = entries[i].name;
                if (fname.endsWith(".js") || fname.endsWith(".lua")) {
                    bool duplicate = false;
                    for (int j = 0; j < appCount; j++) {
                        if (appNames[j] == fname) { duplicate = true; break; }
                    }
                    if (duplicate) continue;
                    
                    appPaths[appCount] = entries[i].path;
                    appNames[appCount] = fname;
                    appCategories[appCount] = "Legacy Scripts";
                    appIsFolder[appCount] = false;
                    appCount++;
                }
            }
        }
    }
}

int LauncherUI::getLauncherItems(LauncherItem* items, int maxItems) {
    int count = 0;

    // NIVEL 1: Menu Principal (Se não houver categoria selecionada)
    if (currentCategory.length() == 0) {
        // Seção System
        if (count < maxItems) items[count++] = {"[ SYSTEM ]", true, ITEM_HEADER, -1, -1, ""};
        if (count < maxItems) items[count++] = {"App Store", false, ITEM_SYS, 1, -1, ""};
        if (count < maxItems) items[count++] = {"App Installer", false, ITEM_SYS, 2, -1, ""};
        if (count < maxItems) items[count++] = {"Settings", false, ITEM_SYS, 3, -1, ""};
        if (count < maxItems) items[count++] = {"Help Center", false, ITEM_SYS, 4, -1, ""};

        // Seção de Categorias
        if (appCount > 0) {
            if (count < maxItems) items[count++] = {"[ CATEGORIES ]", true, ITEM_HEADER, -1, -1, ""};

            // Agrupa e lista categorias únicas disponíveis
            String uniqueCategories[20];
            int categoryCount = 0;

            for (int i = 0; i < appCount; i++) {
                bool found = false;
                for (int c = 0; c < categoryCount; c++) {
                    if (uniqueCategories[c] == appCategories[i]) {
                        found = true;
                        break;
                    }
                }
                if (!found && categoryCount < 20) {
                    uniqueCategories[categoryCount++] = appCategories[i];
                }
            }

            // Adiciona as pastas de categorias à lista
            for (int c = 0; c < categoryCount && count < maxItems; c++) {
                String catName = "> " + uniqueCategories[c];
                items[count++] = {catName, false, ITEM_CATEGORY, -1, -1, uniqueCategories[c]};
            }
        }
    } 
    // NIVEL 2: Submenu da Categoria Selecionada
    else {
        if (count < maxItems) items[count++] = {"[ " + currentCategory + " ]", true, ITEM_HEADER, -1, -1, ""};

        for (int i = 0; i < appCount && count < maxItems; i++) {
            if (appCategories[i] == currentCategory) {
                items[count++] = {appNames[i], false, ITEM_APP, -1, i, ""};
            }
        }
    }

    return count;
}

void LauncherUI::draw() {
    tft.fillScreen(TFT_BLACK);

    if (needsRescan) {
        scanLocalApps();
        needsRescan = false;
        tft.fillScreen(TFT_BLACK);
    }

    if (tft.height() >= 240) {
        drawTall();
    } else {
        drawCompact();
    }
}

// Desenha o Ícone do Wi-Fi dinamizando a cor e os níveis de sinal conforme o RSSI
void drawIconWiFi(int16_t x, int16_t y) {
    if (WiFi.status() == WL_CONNECTED) {
        int32_t rssi = WiFi.RSSI(); // Sinal em dBm
        
        // 1. Determina a cor ativa com base na força do sinal
        uint16_t activeColor = TFT_GREEN;
        if (rssi <= -75) {
            activeColor = TFT_RED;    // Sinal Fraco / Instável
        } else if (rssi <= -60) {
            activeColor = TFT_YELLOW; // Sinal Médio
        } else {
            activeColor = TFT_GREEN;  // Sinal Forte / Excelente
        }

        uint16_t inactiveColor = TFT_DARKGREY; // Cor das barras desligadas

        // 2. Determina a ativação dos arcos
        uint16_t colorDot  = activeColor; 
        uint16_t colorArc1 = (rssi > -85) ? activeColor : inactiveColor; // Nível 1
        uint16_t colorArc2 = (rssi > -70) ? activeColor : inactiveColor; // Nível 2
        uint16_t colorArc3 = (rssi > -55) ? activeColor : inactiveColor; // Nível 3

        // 3. Renderiza os arcos e o ponto central
        tft.drawCircleHelper(x + 6, y + 9, 6, 1, colorArc3); // Arco Externo
        tft.drawCircleHelper(x + 6, y + 9, 4, 1, colorArc2); // Arco Médio
        tft.drawCircleHelper(x + 6, y + 9, 2, 1, colorArc1); // Arco Interno
        tft.fillCircle(x + 6, y + 9, 1, colorDot);           // Ponto Central
    } else {
        // Desconectado: Exibe apenas um 'X' ou ponto vermelho
        tft.fillCircle(x + 6, y + 9, 1, TFT_RED);
    }
}

// Desenha o Ícone do SD Card com variação de cor por uso:
// Vazio (< 10%): Branco
// Normal (10% a 85%): Verde
// Cheio (> 85%): Vermelho
void drawIconSD(int16_t x, int16_t y) {
    if (isSDMounted()) { 
        uint64_t total = getSDTotalBytes();
        uint64_t used = getSDUsedBytes();
        
        // Define a cor padrão como Branco (Vazio / Sem dados suficientes)
        uint16_t color = TFT_WHITE; 

        if (total > 0) {
            // Calcula a porcentagem de ocupação (0 a 100%)
            int usagePercent = (int)((used * 100) / total);

            if (usagePercent > 85) {
                color = TFT_RED;    // Cheio (> 85%)
            } else if (usagePercent >= 10) {
                color = TFT_GREEN;  // Normal (10% - 85%)
            } else {
                color = TFT_WHITE;  // Vazio (< 10%)
            }
        }

        // Se a tela for pequena (ex: 240x135), usa o modelo básico visível
        if (tft.height() <= 135 || tft.width() <= 135) {
            
            // Modelo Básico (240x135):
            // Corpo preenchido para dar contraste (12x14px)
            tft.fillRect(x, y, 12, 16, color);
            
            // Canto chanfrado superior esquerdo (Corta o canto)
            tft.fillRect(x, y, 3, 5, TFT_BLACK);
            tft.drawLine(x + 3, y, x, y + 2, color);

            // Módulo de pinos (Três detalhes pretos no topo para identificar como SD)
            tft.drawFastVLine(x + 4, y + 2, 6, TFT_BLACK);
            tft.drawFastVLine(x + 7, y + 2, 6, TFT_BLACK);
            tft.drawFastVLine(x + 9, y + 2, 6, TFT_BLACK);

        } else {
            
            // Modelo Original Mantido Intacto (240x320)
            tft.drawRect(x, y, 9, 11, color);
            tft.drawLine(x + 2, y, x, y + 2, color); // Canto chanfrado do SD
            
            // Linhas dos pinos do SD
            tft.drawLine(x + 2, y + 2, x + 2, y + 4, color);
            tft.drawLine(x + 4, y + 2, x + 4, y + 4, color);
            tft.drawLine(x + 6, y + 2, x + 6, y + 4, color);
        }
    }
}

// Desenha o Ícone da Bateria dinâmico de acordo com a porcentagem
void drawIconBattery(int16_t x, int16_t y, uint16_t color) {
    // Corpo da bateria (14x8 pixels)
    tft.drawRect(x, y + 2, 14, 8, color);
    // Polo positivo
    tft.fillRect(x + 14, y + 4, 2, 4, color);

    // Obtém o percentual real da placa
    int percent = getBatteryPercent();

    // Mapeia 0 a 100% para uma largura de preenchimento de 0 a 10 pixels
    int fillWidth = map(percent, 0, 100, 0, 10);

    // Define a cor baseada no nível da bateria
    uint16_t fillColor = TFT_GREEN;
    if (percent <= 20) {
        fillColor = TFT_RED;    // Crítico
    } else if (percent <= 50) {
        fillColor = TFT_YELLOW; // Médio
    }

    // Limpa a área interna antes de desenhar o nível
    tft.fillRect(x + 2, y + 4, 10, 4, TFT_BLACK);

    // Desenha o preenchimento proporcional
    if (fillWidth > 0) {
        tft.fillRect(x + 2, y + 4, fillWidth, 4, fillColor);
    }
}

// Função para desenhar toda a barra de ícones no cabeçalho
void drawHeaderIcons(int16_t startX, int16_t y) {
    int currentX = startX;

    // 1. SD Card (lado esquerdo do bloco de ícones)
    if(isSDMounted()){
        drawIconSD(currentX, y);
        currentX += 14;
    }

    // 2. Wi-Fi
    if (WiFi.status() == WL_CONNECTED) {
        drawIconWiFi(currentX, y);
        currentX += 16;
    }

    // 3. Bateria
    if(hasBattery())drawIconBattery(currentX, y, TFT_WHITE);
}

// Renderização para telas 240x320 (Layout em Grade Vertical)
void LauncherUI::drawTall() {
    refreshItems();

    // Moldura principal
    tft.drawRoundRect(3, 3, 234, 314, 5, TFT_WHITE);
    
    // Cabeçalho
    tft.fillRoundRect(6, 6, 228, 30, 5, TFT_BLACK);
    tft.drawRoundRect(6, 6, 228, 30, 5, TFT_GREEN);
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("KryonOS", 14, 14, 2);

    drawHeaderIcons(170, 15);

    int itemsPerPage = 6;
    int startY = 42;
    int cardH = 36;
    int gapY = 4;

    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = scrollOffset + i;
        if (listIndex >= totalItems) break;

        LauncherItem item = items[listIndex];
        int yPos = startY + (i * (cardH + gapY));

        if (item.isHeader) {
            tft.fillRoundRect(8, yPos, 224, cardH, 4, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(item.label.c_str(), 120, yPos + (cardH / 2), 2);
        } else {
            bool isSelected = (listIndex == selectedIndex);
            uint16_t bg = isSelected ? TFT_WHITE : TFT_BLACK;
            uint16_t border = isSelected ? TFT_WHITE : TFT_DARKGREY;
            uint16_t textCol = isSelected ? TFT_BLACK : TFT_WHITE;

            tft.fillRoundRect(8, yPos, 224, cardH, 4, bg);
            tft.drawRoundRect(8, yPos, 224, cardH, 4, border);

            tft.setTextColor(textCol, bg);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(item.label.c_str(), 120, yPos + (cardH / 2), 2);
        }
    }

    // --- RODAPÉ PROPORCIONAL (4 BOTÕES IGUAIS) ---
    int footerX = 5;
    int footerY = 285;
    int footerW = 230;
    int footerH = 26;

    tft.drawRoundRect(footerX, footerY, footerW, footerH, 4, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);

    const char* labels[4] = {"UP", "SEL", "DN", "BACK"};

    for (int i = 0; i < 4; i++) {
        int x1 = footerX + (i * footerW) / 4;
        int x2 = footerX + ((i + 1) * footerW) / 4;
        int centerX = x1 + (x2 - x1) / 2;

        // Renderiza o texto centralizado na sua respectiva fatia
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(labels[i], centerX, footerY + (footerH / 2), 2);

        // Desenha as linhas divisórias entre os botões
        if (i < 3) {
            tft.drawFastVLine(x2, footerY, footerH, TFT_DARKGREY);
        }
    }
}

void LauncherUI::drawCompact() {
    refreshItems(); // Atualiza totalItems usando o buffer global da classe

    // Header Minimalista
    tft.fillRoundRect(2, 2, 236, 18, 3, TFT_DARKGREY);
    
    // Título
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("KryonOS", 6, 4, 2);

    // Ícones de status
    drawHeaderIcons(175, 5);

    int cols = 2;
    int rows = 2;
    int itemsPerPage = cols * rows;
    int cardW = 112;
    int cardH = 34;
    int startY = 24;

    for (int i = 0; i < itemsPerPage; i++) {
        int listIndex = scrollOffset + i;
        if (listIndex >= totalItems) break;

        LauncherItem item = items[listIndex];

        int r = i / cols;
        int c = i % cols;
        int xPos = 5 + c * (cardW + 6);
        int yPos = startY + r * (cardH + 4);

        if (item.isHeader) {
            tft.fillRoundRect(xPos, yPos, cardW, cardH, 4, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(item.label.c_str(), xPos + (cardW / 2), yPos + (cardH / 2), 1);
        } else {
            bool isSelected = (listIndex == selectedIndex);
            uint16_t bg = isSelected ? TFT_WHITE : TFT_BLACK;
            uint16_t border = isSelected ? TFT_WHITE : TFT_DARKGREY;
            uint16_t textCol = isSelected ? TFT_BLACK : TFT_WHITE;

            tft.fillRoundRect(xPos, yPos, cardW, cardH, 4, bg);
            tft.drawRoundRect(xPos, yPos, cardW, cardH, 4, border);

            tft.setTextColor(textCol, bg);
            tft.setTextDatum(MC_DATUM);
            
            String displayName = item.label;
            if (displayName.length() > 12) {
                displayName = displayName.substring(0, 10) + "..";
            }
            tft.drawString(displayName.c_str(), xPos + (cardW / 2), yPos + (cardH / 2), 2);
        }
    }

    // Rodapé minimalista com instrução de retorno (ESC/Back)
    tft.fillRoundRect(5, 104, 230, 26, 4, TFT_NAVY);
    tft.drawRoundRect(5, 104, 230, 26, 4, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Arrows/Enter | ESC: Back", 120, 117, 2);
}

static void runApp(const String& path, bool isFolder)
{
    extern int currentState;
    currentState = 2; // STATE_RUN_APP

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);

    String filePath;

    if (isFolder)
    {
        filePath = path;

        if (!filePath.endsWith("/"))
            filePath += "/";

        // -------------------------------------------------
        // Prioridade:
        //
        // 1. main.luac
        // 2. main.lua
        // 3. main.wren
        // 4. main.js
        // -------------------------------------------------

        if (FileSystem::exists((filePath + "main.luac").c_str()))
        {
            filePath += "main.luac";
        }
        else if (FileSystem::exists((filePath + "main.lua").c_str()))
        {
            filePath += "main.lua";
        }
        else if (FileSystem::exists((filePath + "main.wren").c_str()))
        {
            filePath += "main.wren";
        }
        else if (FileSystem::exists((filePath + "main.js").c_str()))
        {
            filePath += "main.js";
        }
        else
        {
            Serial.println("[App] No main script found.");
            return;
        }
    }
    else
    {
        filePath = path;

        bool knownExtension =
            filePath.endsWith(".lua")  ||
            filePath.endsWith(".luac") ||
            filePath.endsWith(".wren") ||
            filePath.endsWith(".js");

        if (!knownExtension)
        {
            Serial.print("[App] Unknown file type: ");
            Serial.println(filePath);
            return;
        }
    }

    Serial.print("[App] Executando: ");
    Serial.println(filePath);

    HarixKernel::runFile(filePath.c_str());

    tft.fillRoundRect(200, 0, 40, 30, 5, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("X", 220, 15, 2);
}

// Preenche o buffer interno de itens
void LauncherUI::refreshItems() {
    totalItems = getLauncherItems(items, 50);
}

void LauncherUI::goBack() {
    if (currentCategory.length() > 0) {
        // Sai da categoria e retorna ao menu raiz
        currentCategory = "";
        refreshItems();

        // Restaura a seleção para a categoria onde o usuário clicou
        selectedIndex = lastCategoryIndex;
        if (selectedIndex >= totalItems) selectedIndex = 0;

        // Ajusta o scroll para manter o item restaurado visível
        int maxVisible = 3;
        if (selectedIndex >= scrollOffset + maxVisible || selectedIndex < scrollOffset) {
            scrollOffset = (selectedIndex >= maxVisible) ? (selectedIndex - maxVisible + 1) : 0;
        }

        draw();
    } else {
        // Se já está na raiz, sai do Launcher
        extern int currentState;
        currentState = 0;
    }
}

void LauncherUI::executeSelectedItem() {
    extern int currentState;
    refreshItems();

    if (selectedIndex < 0 || selectedIndex >= totalItems) return;

    LauncherItem item = items[selectedIndex];

    switch (item.type) {
        case ITEM_HEADER:
            break;

        case ITEM_CATEGORY:
            // Salva a posição atual antes de entrar na categoria
            lastCategoryIndex = selectedIndex; 
            currentCategory = item.categoryTarget;
            selectedIndex = 0;
            scrollOffset = 0;
            draw();
            break;

        case ITEM_SYS:
            if (item.sysId == 1) currentState = 13;
            else if (item.sysId == 2) currentState = 3;
            else if (item.sysId == 3) currentState = 1;
            else if (item.sysId == 4) currentState = 14;
            break;

        case ITEM_APP:
            if (item.appIndex >= 0 && item.appIndex < appCount) {
                runApp(appPaths[item.appIndex], appIsFolder[item.appIndex]);
            }
            break;
            
        default:
            break;
    }
}

void LauncherUI::navigateUp() {
    refreshItems();
    if (totalItems == 0) return;

    int targetIndex = selectedIndex - 1;

    while (targetIndex >= 0 && items[targetIndex].isHeader) {
        targetIndex--;
    }

    if (targetIndex >= 0) {
        selectedIndex = targetIndex;
    }

    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    }
    
    draw();
}

void LauncherUI::navigateDown() {
    refreshItems();
    int maxVisible = 3;

    int targetIndex = selectedIndex + 1;

    while (targetIndex < totalItems && items[targetIndex].isHeader) {
        targetIndex++;
    }

    if (targetIndex < totalItems) {
        selectedIndex = targetIndex;
    }

    if (selectedIndex >= scrollOffset + maxVisible) {
        scrollOffset = selectedIndex - (maxVisible - 1);
    }
    
    draw();
}

void LauncherUI::handleKeyInput(BoardKey key) {
    if (key == BOARD_KEY_UP) {
        navigateUp();
    } 
    else if (key == BOARD_KEY_DOWN) {
        navigateDown();
    } 
    else if (key == BOARD_KEY_ENTER) {
        executeSelectedItem();
    } 
    else if (key == BOARD_KEY_ESC) {
        goBack(); // Usa a lógica unificada de retorno
    }
}

void LauncherUI::handleTouch(uint16_t x, uint16_t y) {
    refreshItems();

    // 1. TOUCH NOS ITENS DA LISTA
    if (y >= 45 && y <= 270) {
        int clickedRelativeIndex = (y - 45) / 30;
        int clickedAbsoluteIndex = scrollOffset + clickedRelativeIndex;

        if (clickedAbsoluteIndex < totalItems && !items[clickedAbsoluteIndex].isHeader) {
            selectedIndex = clickedAbsoluteIndex;
            draw();
            executeSelectedItem();
        }
        return;
    }

    // 2. TOUCH NOS BOTÕES DO RODAPÉ (Calcula a fatia proporcional baseada no X)
    if (y >= 285 && y <= 315) {
        uint16_t screenWidth = tft.width(); // Largura total do display
        int btnIndex = x / (screenWidth / 4);

        switch (btnIndex) {
            case 0:
                navigateUp();
                break;
            case 1:
                executeSelectedItem();
                break;
            case 2:
                navigateDown();
                break;
            case 3:
            default:
                goBack();
                break;
        }
        return;
    }

    // 3. TOUCH NO HEADER
    if (y < 40) {
        goBack();
        return;
    }
}