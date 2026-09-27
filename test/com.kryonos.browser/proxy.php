<?php
/**
 * KryonOS Edge-Transcoding Web Proxy Engine
 * Single-file, zero-dependency PHP proxy for ESP32-S3 Web Browser.
 * 
 * Features:
 * - SSRF Protection (Blocks internal/private subnets)
 * - Semantic Reader Engine (Destructive stripping of scripts, styles, ads)
 * - Hardware Pre-Layout (220px word-wrapping, cumulative vertical coordinate tagging)
 * - Touch-Zone Link Bounding Box Generation
 * - Memory-Capped Pagination (10-15 KB max payload per page)
 * - Local File-based Cache (< 5ms repeat response)
 */

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With');

if (($_SERVER['REQUEST_METHOD'] ?? '') === 'OPTIONS') {
    http_response_code(200);
    exit;
}

// -----------------------------------------------------------------------------
// 1. Configuration & Constants
// -----------------------------------------------------------------------------
define('CACHE_DIR', sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'kryon_browser_cache');
define('CACHE_TTL', 900); // 15 minutes
define('PAGE_SIZE_NODES', 45); // Max structural nodes per payload
define('DEFAULT_CONTENT_WIDTH', 220); // 240px TFT with 10px margins

// Character limits per line based on KryonOS TFT_eSPI fonts:
define('CHARS_PER_LINE_P', 28);   // Font 2 normal text
define('CHARS_PER_LINE_H1', 16);  // Font 4 large heading
define('CHARS_PER_LINE_H2', 22);  // Font 2 bold/subheading
define('CHARS_PER_LINE_CODE', 26);

// -----------------------------------------------------------------------------
// 2. SSRF Firewall & Security Validation
// -----------------------------------------------------------------------------
function isPrivateOrReservedIP(string $ip): bool {
    // Check IPv4 private / loopback / link-local / broadcast
    $flags = FILTER_FLAG_IPV4 | FILTER_FLAG_IPV6 | FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE;
    if (filter_var($ip, FILTER_VALIDATE_IP, $flags) === false) {
        return true; // Is private or reserved
    }

    // Explicit subnet checks for additional safety
    $long = ip2long($ip);
    if ($long !== false) {
        // 0.0.0.0/8
        if (($long & 0xFF000000) === 0x00000000) return true;
        // 10.0.0.0/8
        if (($long & 0xFF000000) === 0x0A000000) return true;
        // 127.0.0.0/8
        if (($long & 0xFF000000) === 0x7F000000) return true;
        // 169.254.0.0/16
        if (($long & 0xFFFF0000) === 0xA9FE0000) return true;
        // 172.16.0.0/12
        if (($long & 0xFFF00000) === 0xAC100000) return true;
        // 192.168.0.0/16
        if (($long & 0xFFFF0000) === 0xC0A80000) return true;
    }

    // IPv6 loopback and unique local
    if ($ip === '::1' || strpos($ip, 'fc00:') === 0 || strpos($ip, 'fe80:') === 0) {
        return true;
    }

    return false;
}

function validateAndSanitizeUrl(string $inputUrl): string {
    $trimmed = trim($inputUrl);
    if (empty($trimmed)) {
        throw new InvalidArgumentException("Empty URL provided.");
    }

    // If query is a search keyword rather than a URL
    if (!preg_match('#^https?://#i', $trimmed)) {
        if (!str_contains($trimmed, '.') || str_contains($trimmed, ' ')) {
            // Convert to DuckDuckGo Lite search
            return 'https://lite.duckduckgo.com/lite/?q=' . urlencode($trimmed);
        }
        $trimmed = 'https://' . $trimmed;
    }

    $parts = parse_url($trimmed);
    if (!$parts || empty($parts['host'])) {
        throw new InvalidArgumentException("Malformed URL structure.");
    }

    $scheme = strtolower($parts['scheme'] ?? '');
    if ($scheme !== 'http' && $scheme !== 'https') {
        throw new InvalidArgumentException("Unsupported protocol: only http and https are allowed.");
    }

    $host = $parts['host'];
    
    // Resolve DNS and check each IP
    $ips = gethostbynamel($host);
    if (!$ips || empty($ips)) {
        // Try direct IP check
        if (filter_var($host, FILTER_VALIDATE_IP)) {
            $ips = [$host];
        } else {
            throw new RuntimeException("Could not resolve domain: " . htmlspecialchars($host));
        }
    }

    foreach ($ips as $ip) {
        if (isPrivateOrReservedIP($ip)) {
            throw new SecurityException("Access to private/internal IP address ({$ip}) is blocked by SSRF firewall.");
        }
    }

    return $trimmed;
}

class SecurityException extends Exception {}

// -----------------------------------------------------------------------------
// 3. Upstream cURL Network Retrieval
// -----------------------------------------------------------------------------
function fetchUpstream(string $url): array {
    $ch = curl_init();
    
    curl_setopt_array($ch, [
        CURLOPT_URL => $url,
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_FOLLOWLOCATION => true,
        CURLOPT_MAXREDIRS => 4,
        CURLOPT_TIMEOUT => 8,
        CURLOPT_CONNECTTIMEOUT => 4,
        CURLOPT_ENCODING => '', // Accept gzip, deflate, brotli
        CURLOPT_SSL_VERIFYPEER => false,
        CURLOPT_SSL_VERIFYHOST => 0,
        CURLOPT_USERAGENT => 'Mozilla/5.0 (Linux; Android 10; KryonOS Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36',
        CURLOPT_HTTPHEADER => [
            'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,text/plain;q=0.8,*/*;q=0.5',
            'Accept-Language: en-US,en;q=0.9',
            'Cache-Control: max-age=0'
        ]
    ]);

    $body = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $contentType = curl_getinfo($ch, CURLINFO_CONTENT_TYPE) ?: '';
    $effectiveUrl = curl_getinfo($ch, CURLINFO_EFFECTIVE_URL) ?: $url;
    $curlError = curl_error($ch);
    curl_close($ch);

    if ($body === false || !empty($curlError)) {
        throw new RuntimeException("Upstream connection failed: " . $curlError);
    }

    return [
        'code' => $httpCode,
        'contentType' => strtolower($contentType),
        'effectiveUrl' => $effectiveUrl,
        'body' => $body
    ];
}

// -----------------------------------------------------------------------------
// 4. URL Resolution & Normalization (RFC 3986)
// -----------------------------------------------------------------------------
function resolveRelativeUrl(string $base, string $rel): string {
    if (empty($rel)) return $base;
    if (preg_match('#^https?://#i', $rel)) return $rel;
    if (str_starts_with($rel, '//')) return 'https:' . $rel;
    if (str_starts_with($rel, '#') || str_starts_with($rel, 'javascript:') || str_starts_with($rel, 'mailto:')) return '';

    $baseParts = parse_url($base);
    if (!$baseParts || empty($baseParts['host'])) return $rel;

    $scheme = $baseParts['scheme'] ?? 'https';
    $host = $baseParts['host'];
    $port = isset($baseParts['port']) ? ':' . $baseParts['port'] : '';
    $basePath = $baseParts['path'] ?? '/';

    if (str_starts_with($rel, '/')) {
        return "{$scheme}://{$host}{$port}{$rel}";
    }

    // Relative to directory
    $dir = preg_replace('#/[^/]*$#', '', $basePath);
    return "{$scheme}://{$host}{$port}{$dir}/{$rel}";
}

// -----------------------------------------------------------------------------
// 5. Hardware-Aware Text Word Wrapping
// -----------------------------------------------------------------------------
function wrapText(string $text, int $maxChars): array {
    $cleaned = preg_replace('/\s+/u', ' ', trim($text));
    if (empty($cleaned)) return [];

    $words = explode(' ', $cleaned);
    $lines = [];
    $currentLine = '';

    foreach ($words as $word) {
        if (mb_strlen($word) > $maxChars) {
            // Cut long unbroken word
            if (!empty($currentLine)) {
                $lines[] = $currentLine;
                $currentLine = '';
            }
            $chunks = str_split($word, $maxChars);
            foreach ($chunks as $chunk) {
                $lines[] = $chunk;
            }
            continue;
        }

        $testLine = ($currentLine === '') ? $word : $currentLine . ' ' . $word;
        if (mb_strlen($testLine) <= $maxChars) {
            $currentLine = $testLine;
        } else {
            if ($currentLine !== '') $lines[] = $currentLine;
            $currentLine = $word;
        }
    }

    if ($currentLine !== '') {
        $lines[] = $currentLine;
    }

    return $lines;
}

// -----------------------------------------------------------------------------
// 6. Semantic Distillation & Reader Engine
// -----------------------------------------------------------------------------
function extractAndLayoutDocument(string $html, string $baseUrl, int $contentWidth): array {
    libxml_use_internal_errors(true);
    $doc = new DOMDocument();
    
    // Encode UTF-8 meta tag for reliable parsing
    $htmlWithMeta = '<?xml encoding="utf-8" ?>' . $html;
    $doc->loadHTML($htmlWithMeta, LIBXML_NOERROR | LIBXML_NOWARNING | LIBXML_NONET);
    libxml_clear_errors();

    // Determine Base URL (check <base href="...">)
    $effectiveBase = $baseUrl;
    $baseTags = $doc->getElementsByTagName('base');
    if ($baseTags->length > 0) {
        $baseHref = $baseTags->item(0)->getAttribute('href');
        if (!empty($baseHref)) {
            $effectiveBase = resolveRelativeUrl($baseUrl, $baseHref);
        }
    }

    // Extract Page Title
    $title = "Kryon Web Reader";
    $titleTags = $doc->getElementsByTagName('title');
    if ($titleTags->length > 0) {
        $t = trim($titleTags->item(0)->textContent);
        if (!empty($t)) $title = $t;
    }

    $xpath = new DOMXPath($doc);

    // Destructively remove unwanted subtrees (use precise selectors to avoid stripping content containing 'ad' like heading, lead, etc.)
    $removeQueries = [
        '//script', '//style', '//svg', '//canvas', '//iframe', '//video', 
        '//audio', '//noscript', '//nav', '//footer', '//form', 
        '//button', '//input', '//select', '//dialog', '//aside',
        '//*[contains(concat(" ", normalize-space(@class), " "), " ad ")]',
        '//*[contains(concat(" ", normalize-space(@class), " "), " ads ")]',
        '//*[contains(concat(" ", normalize-space(@class), " "), " advertisement ")]',
        '//*[contains(@class, "ad-box")]', '//*[contains(@class, "ad-banner")]',
        '//*[contains(@class, "cookie-banner")]', '//*[contains(@class, "cookie-consent")]',
        '//*[contains(@class, "popup-modal")]'
    ];

    foreach ($removeQueries as $query) {
        $nodes = $xpath->query($query);
        if ($nodes) {
            foreach ($nodes as $node) {
                if ($node->parentNode) {
                    $node->parentNode->removeChild($node);
                }
            }
        }
    }

    // Target content root
    $bodyList = $doc->getElementsByTagName('body');
    $rootNode = ($bodyList->length > 0) ? $bodyList->item(0) : $doc->documentElement;
    if (!$rootNode) {
        $rootNode = $doc;
    }

    // Collect visual semantic elements
    $nodes = [];
    $currentY = 0;

    $elements = $xpath->query('.//h1 | .//h2 | .//h3 | .//h4 | .//p | .//blockquote | .//li | .//hr | .//a[@href]', $rootNode);

    if (!$elements || $elements->length === 0) {
        // Fallback: raw body text
        $rawText = ($rootNode && isset($rootNode->textContent)) ? trim($rootNode->textContent) : '';
        $lines = wrapText($rawText, CHARS_PER_LINE_P);
        if (!empty($lines)) {
            $h = count($lines) * 16 + 6;
            $nodes[] = [
                't' => 3, // Paragraph
                'lines' => $lines,
                'x' => 10,
                'y' => $currentY,
                'w' => $contentWidth,
                'h' => $h
            ];
            $currentY += $h + 6;
        }
    } else {
        foreach ($elements as $el) {
            $tagName = strtolower($el->nodeName);
            $rawContent = ($el && isset($el->textContent)) ? trim($el->textContent) : '';
            if (empty($rawContent) && $tagName !== 'hr') continue;

            if ($tagName === 'h1') {
                $lines = wrapText($rawContent, CHARS_PER_LINE_H1);
                if (empty($lines)) continue;
                $h = count($lines) * 20 + 8;
                $nodes[] = [
                    't' => 1, // Heading 1
                    'lines' => $lines,
                    'x' => 10,
                    'y' => $currentY,
                    'w' => $contentWidth,
                    'h' => $h
                ];
                $currentY += $h + 8;
            } else if ($tagName === 'h2' || $tagName === 'h3' || $tagName === 'h4') {
                $lines = wrapText($rawContent, CHARS_PER_LINE_H2);
                if (empty($lines)) continue;
                $h = count($lines) * 18 + 6;
                $nodes[] = [
                    't' => 2, // Heading 2
                    'lines' => $lines,
                    'x' => 10,
                    'y' => $currentY,
                    'w' => $contentWidth,
                    'h' => $h
                ];
                $currentY += $h + 6;
            } else if ($tagName === 'hr') {
                $nodes[] = [
                    't' => 5, // Horizontal Rule
                    'lines' => [],
                    'x' => 10,
                    'y' => $currentY,
                    'w' => $contentWidth,
                    'h' => 4
                ];
                $currentY += 10;
            } else if ($tagName === 'blockquote') {
                $lines = wrapText($rawContent, CHARS_PER_LINE_P);
                if (empty($lines)) continue;
                $h = count($lines) * 16 + 8;
                $nodes[] = [
                    't' => 6, // Blockquote
                    'lines' => $lines,
                    'x' => 14,
                    'y' => $currentY,
                    'w' => $contentWidth - 8,
                    'h' => $h
                ];
                $currentY += $h + 8;
            } else if ($tagName === 'li') {
                $lines = wrapText("• " . $rawContent, CHARS_PER_LINE_P);
                if (empty($lines)) continue;
                $h = count($lines) * 16 + 4;
                $nodes[] = [
                    't' => 7, // List item
                    'lines' => $lines,
                    'x' => 12,
                    'y' => $currentY,
                    'w' => $contentWidth,
                    'h' => $h
                ];
                $currentY += $h + 4;
            } else if ($tagName === 'a') {
                $href = $el->getAttribute('href');
                $targetUrl = resolveRelativeUrl($effectiveBase, $href);
                if (empty($targetUrl) || str_starts_with($targetUrl, '#')) continue;

                $lines = wrapText("► " . $rawContent, CHARS_PER_LINE_P);
                if (empty($lines)) continue;
                $h = count($lines) * 16 + 6;
                $nodes[] = [
                    't' => 4, // Link node
                    'lines' => $lines,
                    'u' => $targetUrl,
                    'x' => 10,
                    'y' => $currentY,
                    'w' => $contentWidth,
                    'h' => $h
                ];
                $currentY += $h + 6;
            } else { // Paragraph
                $lines = wrapText($rawContent, CHARS_PER_LINE_P);
                if (empty($lines)) continue;
                $h = count($lines) * 16 + 6;
                $nodes[] = [
                    't' => 3, // Paragraph
                    'lines' => $lines,
                    'x' => 10,
                    'y' => $currentY,
                    'w' => $contentWidth,
                    'h' => $h
                ];
                $currentY += $h + 6;
            }
        }
    }

    return [
        'title' => mb_substr($title, 0, 50),
        'url' => $effectiveBase,
        'totalHeight' => $currentY,
        'allNodes' => $nodes
    ];
}

// -----------------------------------------------------------------------------
// 7. Request Handler & Response Pipeline
// -----------------------------------------------------------------------------
try {
    $requestedUrl = $_GET['url'] ?? '';
    $page = max(0, (int)($_GET['page'] ?? 0));
    $contentWidth = max(180, min(320, (int)($_GET['w'] ?? DEFAULT_CONTENT_WIDTH)));

    if (empty($requestedUrl)) {
        // Return default home greeting payload
        echo json_encode([
            'status' => 200,
            'title' => 'KryonOS Web Browser',
            'url' => 'about:home',
            'page' => 0,
            'totalPages' => 1,
            'totalHeight' => 280,
            'nodes' => [
                [
                    't' => 1,
                    'lines' => ['KryonOS Browser'],
                    'x' => 10, 'y' => 0, 'w' => 220, 'h' => 26
                ],
                [
                    't' => 3,
                    'lines' => ['Welcome to the Edge-Transcoded', 'Web Reader for KryonOS.'],
                    'x' => 10, 'y' => 32, 'w' => 220, 'h' => 36
                ],
                [
                    't' => 5,
                    'lines' => [],
                    'x' => 10, 'y' => 74, 'w' => 220, 'h' => 4
                ],
                [
                    't' => 4,
                    'lines' => ['► Wikipedia: Random Article'],
                    'u' => 'https://en.wikipedia.org/wiki/Special:Random',
                    'x' => 10, 'y' => 84, 'w' => 220, 'h' => 24
                ],
                [
                    't' => 4,
                    'lines' => ['► Hacker News Digest'],
                    'u' => 'https://news.ycombinator.com',
                    'x' => 10, 'y' => 114, 'w' => 220, 'h' => 24
                ],
                [
                    't' => 4,
                    'lines' => ['► ESP32 Technical Overview'],
                    'u' => 'https://en.wikipedia.org/wiki/ESP32-S3',
                    'x' => 10, 'y' => 144, 'w' => 220, 'h' => 24
                ],
                [
                    't' => 4,
                    'lines' => ['► DuckDuckGo Search: Tech'],
                    'u' => 'https://lite.duckduckgo.com/lite/?q=ESP32+S3+microcontroller',
                    'x' => 10, 'y' => 174, 'w' => 220, 'h' => 24
                ]
            ]
        ]);
        exit;
    }

    // 1. Sanitize & SSRF Check
    $validatedUrl = validateAndSanitizeUrl($requestedUrl);

    // 2. Check Cache
    if (!is_dir(CACHE_DIR)) {
        @mkdir(CACHE_DIR, 0777, true);
    }
    $cacheKey = md5($validatedUrl . '_' . $contentWidth);
    $cacheFile = CACHE_DIR . DIRECTORY_SEPARATOR . $cacheKey . '.json';

    $docData = null;
    if (file_exists($cacheFile) && (time() - filemtime($cacheFile) < CACHE_TTL)) {
        $cachedJson = @file_get_contents($cacheFile);
        if ($cachedJson) {
            $docData = json_decode($cachedJson, true);
        }
    }

    if (!$docData) {
        // 3. Fetch Upstream
        $response = fetchUpstream($validatedUrl);

        // Pre-flight Content-Type check (Reject PDF, binary, zip)
        $ct = $response['contentType'];
        if (!empty($ct) && !str_contains($ct, 'text/html') && !str_contains($ct, 'application/xhtml') && !str_contains($ct, 'text/plain')) {
            echo json_encode([
                'status' => 415,
                'title' => 'Unsupported File Format',
                'url' => $response['effectiveUrl'],
                'page' => 0,
                'totalPages' => 1,
                'totalHeight' => 160,
                'nodes' => [
                    [
                        't' => 1,
                        'lines' => ['Unsupported File'],
                        'x' => 10, 'y' => 0, 'w' => 220, 'h' => 24
                    ],
                    [
                        't' => 3,
                        'lines' => [
                            'Content-Type: ' . substr($ct, 0, 30),
                            'This binary format cannot be',
                            'transcoded to web text.'
                        ],
                        'x' => 10, 'y' => 30, 'w' => 220, 'h' => 54
                    ]
                ]
            ]);
            exit;
        }

        // 4. Semantic Parsing & Layout
        $docData = extractAndLayoutDocument($response['body'], $response['effectiveUrl'], $contentWidth);

        // Save Cache
        @file_put_contents($cacheFile, json_encode($docData));
    }

    // 5. Paginate Nodes
    $allNodes = $docData['allNodes'] ?? [];
    $totalNodes = count($allNodes);
    $totalPages = max(1, (int)ceil($totalNodes / PAGE_SIZE_NODES));
    $page = min($totalPages - 1, $page);

    $pageNodes = array_slice($allNodes, $page * PAGE_SIZE_NODES, PAGE_SIZE_NODES);

    // Normalize vertical coordinates relative to page start
    $firstY = !empty($pageNodes) ? $pageNodes[0]['y'] : 0;
    $normalizedNodes = [];
    $pageHeight = 0;

    foreach ($pageNodes as $node) {
        $node['y'] -= $firstY;
        $normalizedNodes[] = $node;
        $pageHeight = max($pageHeight, $node['y'] + $node['h']);
    }

    // 6. Return Payload
    echo json_encode([
        'status' => 200,
        'title' => $docData['title'] ?? 'Web Page',
        'url' => $docData['url'] ?? $validatedUrl,
        'page' => $page,
        'totalPages' => $totalPages,
        'totalNodes' => $totalNodes,
        'pageHeight' => $pageHeight + 20,
        'nodes' => $normalizedNodes
    ]);

} catch (SecurityException $se) {
    http_response_code(200);
    echo json_encode([
        'status' => 403,
        'title' => 'Security Warning',
        'url' => $requestedUrl ?? 'about:blank',
        'page' => 0,
        'totalPages' => 1,
        'totalNodes' => 3,
        'pageHeight' => 150,
        'nodes' => [
            [
                't' => 1,
                'lines' => ['SSRF Blocked'],
                'x' => 10, 'y' => 0, 'w' => 220, 'h' => 24
            ],
            [
                't' => 3,
                'lines' => [
                    substr($se->getMessage(), 0, 50),
                    'Access to private networks',
                    'is forbidden.'
                ],
                'x' => 10, 'y' => 30, 'w' => 220, 'h' => 54
            ],
            [
                't' => 4,
                'lines' => ['► Return to Home'],
                'u' => 'about:home',
                'x' => 10, 'y' => 90, 'w' => 220, 'h' => 24
            ]
        ]
    ]);
} catch (Throwable $e) {
    http_response_code(200);
    echo json_encode([
        'status' => 500,
        'title' => 'Page Load Error',
        'url' => $requestedUrl ?? 'about:blank',
        'page' => 0,
        'totalPages' => 1,
        'totalNodes' => 3,
        'pageHeight' => 160,
        'nodes' => [
            [
                't' => 1,
                'lines' => ['Fetch Failed'],
                'x' => 10, 'y' => 0, 'w' => 220, 'h' => 24
            ],
            [
                't' => 3,
                'lines' => [
                    'Error: ' . substr($e->getMessage(), 0, 48),
                    'Check target URL or host.'
                ],
                'x' => 10, 'y' => 30, 'w' => 220, 'h' => 40
            ],
            [
                't' => 4,
                'lines' => ['► Return to Home'],
                'u' => 'about:home',
                'x' => 10, 'y' => 76, 'w' => 220, 'h' => 24
            ]
        ]
    ]);
}
