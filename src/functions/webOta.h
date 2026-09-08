#ifndef WEB_OTA_H
#define WEB_OTA_H

#include <Arduino.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

static volatile bool gWebOtaUploadSuccess = false;
static volatile bool gWebOtaUploadStarted = false;
static String gWebOtaLastError;

static const char PV_ROUTER_OTA_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#0b1220">
  <meta name="application-name" content="PV Router">
  <link rel="icon" href="/favicon.svg" type="image/svg+xml">
  <title>PV Router · Mise à jour OTA</title>
  <style>
    :root{--bg:#0b1220;--panel:#121c2d;--panel2:#18243a;--line:#263650;--text:#f4f7fb;--muted:#93a4bd;--green:#45d483;--cyan:#48c7ef;--orange:#ffb347;--red:#ff6470;--yellow:#f8df5a}
    *{box-sizing:border-box}body{margin:0;min-height:100vh;background:radial-gradient(circle at top,#17243a 0,#0b1220 38rem);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif}.wrap{max-width:760px;margin:auto;padding:24px 16px 46px}.top{display:flex;align-items:center;justify-content:space-between;gap:14px;margin-bottom:22px}.brand{display:flex;align-items:center;gap:12px}.logo{width:48px;height:48px;border-radius:15px;background:linear-gradient(145deg,#42e686,#48c7ef);display:grid;place-items:center;color:#07130d;font-size:22px;font-weight:900;box-shadow:0 10px 30px #0007}h1{font-size:22px;margin:0}.sub{font-size:13px;color:var(--muted);margin-top:4px}.back{border:1px solid var(--line);background:#17243a;color:var(--text);padding:10px 13px;border-radius:11px;text-decoration:none}.card{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);border-radius:20px;padding:22px;box-shadow:0 16px 42px #0005}.head{display:flex;align-items:flex-start;justify-content:space-between;gap:16px;margin-bottom:18px}.head h2{font-size:18px;margin:0 0 6px}.pill{border:1px solid #2d6b50;background:#173927;color:#8ee8b2;padding:6px 9px;border-radius:999px;font-size:12px;font-weight:800;white-space:nowrap}.muted{color:var(--muted);font-size:13px;line-height:1.5}.drop{border:1.5px dashed #466488;background:#0d1728;border-radius:16px;padding:30px 18px;text-align:center;cursor:pointer;transition:.18s}.drop:hover,.drop.drag{border-color:var(--cyan);background:#10213a;transform:translateY(-1px)}.uploadIcon{width:58px;height:58px;border-radius:18px;background:#18314d;margin:0 auto 14px;display:grid;place-items:center;font-size:27px;color:var(--cyan)}.drop strong{display:block;font-size:16px;margin-bottom:6px}.file{margin-top:14px;border:1px solid var(--line);background:#0d1728;border-radius:13px;padding:12px 13px;display:none;align-items:center;justify-content:space-between;gap:12px}.file.show{display:flex}.fileName{font-weight:750;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.fileSize{font-size:12px;color:var(--muted);white-space:nowrap}.progressWrap{display:none;margin-top:18px}.progressWrap.show{display:block}.progressTop{display:flex;justify-content:space-between;gap:12px;font-size:13px;margin-bottom:8px}.bar{height:13px;border-radius:999px;background:#09111d;border:1px solid var(--line);overflow:hidden}.fill{height:100%;width:0;background:linear-gradient(90deg,var(--cyan),var(--green));transition:width .18s;border-radius:999px}.notice{display:flex;gap:10px;margin-top:16px;padding:12px 13px;border-radius:12px;background:#30270f;border:1px solid #6b5722;color:#f5dfa1;font-size:13px;line-height:1.45}.actions{display:flex;gap:10px;margin-top:18px}.btn{flex:1;border:1px solid var(--line);padding:13px 16px;border-radius:12px;font:inherit;font-weight:800;cursor:pointer}.btn.primary{background:#244b3a;border-color:#347354;color:#b7f3d0}.btn.primary:disabled{opacity:.45;cursor:not-allowed}.btn.secondary{background:#17243a;color:var(--text)}.status{display:none;margin-top:18px;padding:16px;border-radius:14px;border:1px solid var(--line)}.status.show{display:block}.status.ok{background:#133523;border-color:#286244}.status.err{background:#3a1b20;border-color:#6d2a34}.status h3{margin:0 0 7px;font-size:16px}.status.ok h3{color:#8ee8b2}.status.err h3{color:#ff9aa2}.spinner{display:inline-block;width:14px;height:14px;border:2px solid #ffffff3a;border-top-color:#fff;border-radius:50%;animation:spin .8s linear infinite;margin-right:7px;vertical-align:-2px}@keyframes spin{to{transform:rotate(360deg)}}.foot{margin-top:14px;color:var(--muted);font-size:12px;text-align:center}.hidden{display:none}@media(max-width:580px){.top{align-items:flex-start;flex-direction:column}.back{align-self:flex-start}.card{padding:17px}.head{flex-direction:column}.actions{flex-direction:column}}
  </style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <div class="brand"><div class="logo">PV</div><div><h1>Mise à jour OTA</h1><div class="sub">PV Router · <span id="fw">firmware actuel…</span></div></div></div>
    <a class="back" href="/">← Dashboard</a>
  </div>

  <section class="card">
    <div class="head"><div><h2>Installer un nouveau firmware</h2><div class="muted">Sélectionne le fichier <strong>.bin</strong> généré par PlatformIO. La configuration SPIFFS est conservée.</div></div><div class="pill">Firmware uniquement</div></div>

    <input class="hidden" id="fileInput" type="file" accept=".bin,application/octet-stream">
    <div class="drop" id="dropZone" tabindex="0">
      <div class="uploadIcon">↑</div>
      <strong>Déposer le firmware ici</strong>
      <div class="muted">ou toucher pour choisir le fichier <code>firmware.bin</code></div>
    </div>

    <div class="file" id="fileInfo"><div><div class="fileName" id="fileName">—</div><div class="fileSize" id="fileMeta">—</div></div><button class="btn secondary" style="flex:0 0 auto;padding:8px 10px" id="changeBtn">Changer</button></div>

    <div class="progressWrap" id="progressWrap"><div class="progressTop"><span id="progressLabel">Envoi du firmware…</span><strong id="progressPct">0 %</strong></div><div class="bar"><div class="fill" id="progressFill"></div></div></div>

    <div class="notice"><span>⚠</span><div><strong>Ne coupe pas l’alimentation pendant la mise à jour.</strong><br>Après validation du firmware, le PV Router redémarrera automatiquement.</div></div>

    <div class="actions"><button class="btn primary" id="uploadBtn" disabled>Installer le firmware</button><a class="btn secondary" href="/" style="text-align:center;text-decoration:none">Annuler</a></div>

    <div class="status" id="statusBox"><h3 id="statusTitle"></h3><div class="muted" id="statusText"></div></div>
  </section>
  <div class="foot">Interface locale · aucune connexion Internet requise</div>
</div>
<script>
const input=document.getElementById('fileInput'),drop=document.getElementById('dropZone'),info=document.getElementById('fileInfo'),nameEl=document.getElementById('fileName'),meta=document.getElementById('fileMeta'),upload=document.getElementById('uploadBtn'),change=document.getElementById('changeBtn'),wrap=document.getElementById('progressWrap'),fill=document.getElementById('progressFill'),pct=document.getElementById('progressPct'),label=document.getElementById('progressLabel'),box=document.getElementById('statusBox'),title=document.getElementById('statusTitle'),text=document.getElementById('statusText');
let selected=null,busy=false;
fetch('/api/status',{cache:'no-store'}).then(r=>r.json()).then(s=>document.getElementById('fw').textContent=s.firmware_version||s.version||'PV Router').catch(()=>document.getElementById('fw').textContent='PV Router');
function choose(f){if(!f||busy)return;if(!f.name.toLowerCase().endsWith('.bin')){showError('Fichier invalide','Choisis un firmware PlatformIO au format .bin.');return}selected=f;nameEl.textContent=f.name;meta.textContent=(f.size/1024/1024).toFixed(2)+' Mo';info.classList.add('show');box.className='status';upload.disabled=false}
function openPicker(){if(!busy)input.click()}drop.onclick=openPicker;drop.onkeydown=e=>{if(e.key==='Enter'||e.key===' ')openPicker()};input.onchange=()=>choose(input.files[0]);change.onclick=openPicker;['dragenter','dragover'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.add('drag')}));['dragleave','drop'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.remove('drag')}));drop.addEventListener('drop',e=>choose(e.dataTransfer.files[0]));
function showError(t,m){box.className='status show err';title.textContent=t;text.textContent=m}
function waitForRouter(){let tries=0;label.innerHTML='<span class="spinner"></span>Redémarrage du PV Router…';pct.textContent='100 %';fill.style.width='100%';const timer=setInterval(()=>{tries++;fetch('/api/status?ota='+Date.now(),{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(()=>{clearInterval(timer);label.textContent='PV Router de nouveau en ligne';text.textContent='Redémarrage terminé. Retour au dashboard…';setTimeout(()=>location.href='/',900)}).catch(()=>{if(tries>45){clearInterval(timer);text.textContent='Le firmware a été installé. Le redémarrage prend plus de temps que prévu ; recharge la page dans quelques instants.'}})},1200)}
upload.onclick=()=>{if(!selected||busy)return;busy=true;upload.disabled=true;change.disabled=true;drop.style.pointerEvents='none';box.className='status';wrap.classList.add('show');fill.style.width='0%';pct.textContent='0 %';label.textContent='Envoi du firmware…';const form=new FormData();form.append('firmware',selected,selected.name);const xhr=new XMLHttpRequest();xhr.open('POST','/update',true);xhr.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.min(99,Math.round(e.loaded/e.total*100));fill.style.width=p+'%';pct.textContent=p+' %'}};xhr.onload=()=>{let data={};try{data=JSON.parse(xhr.responseText)}catch(e){}if(xhr.status===200&&data.ok){fill.style.width='100%';pct.textContent='100 %';box.className='status show ok';title.textContent='Firmware installé';text.textContent='Validation réussie. Le redémarrage automatique commence maintenant…';waitForRouter()}else{busy=false;upload.disabled=false;change.disabled=false;drop.style.pointerEvents='';showError('Échec de la mise à jour',data.error||('Erreur HTTP '+xhr.status))}};xhr.onerror=()=>{busy=false;upload.disabled=false;change.disabled=false;drop.style.pointerEvents='';showError('Connexion interrompue','Le transfert n’a pas abouti. Le PV Router n’a pas validé le nouveau firmware.')};xhr.send(form)};
</script>
</body>
</html>
)HTML";

static void webOtaRestartTask(void *parameter)
{
  (void)parameter;
  vTaskDelay(2200 / portTICK_PERIOD_MS);
  Serial.println(F("[OTA-WEB] Rebooting after successful firmware update"));
  ESP.restart();
  vTaskDelete(NULL);
}

static void setupWebOta()
{
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html; charset=utf-8", PV_ROUTER_OTA_PAGE);
  });

  server.on(
      "/update",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (gWebOtaUploadSuccess) {
          request->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
          xTaskCreate(webOtaRestartTask, "OTA restart", 2048, NULL, 1, NULL);
        }
        else {
          String payload = "{\"ok\":false,\"error\":\"";
          payload += gWebOtaLastError.length() ? gWebOtaLastError : String("Firmware non valide ou ecriture impossible");
          payload += "\"}";
          request->send(500, "application/json", payload);
        }
      },
      [](AsyncWebServerRequest *request,
         String filename,
         size_t index,
         uint8_t *data,
         size_t len,
         bool final) {
        (void)request;

        if (index == 0) {
          gWebOtaUploadStarted = true;
          gWebOtaUploadSuccess = false;
          gWebOtaLastError = "";
          Serial.printf("[OTA-WEB] Start firmware upload: %s\n", filename.c_str());

          if (!filename.endsWith(".bin")) {
            gWebOtaLastError = "Le fichier doit etre un firmware .bin";
            return;
          }

          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            gWebOtaLastError = "Impossible de preparer la partition OTA";
            Update.printError(Serial);
            return;
          }
        }

        if (gWebOtaLastError.length() == 0 && !Update.hasError() && len > 0) {
          const size_t written = Update.write(data, len);
          if (written != len) {
            gWebOtaLastError = "Erreur pendant l'ecriture du firmware";
            Update.printError(Serial);
          }
        }

        if (final) {
          if (gWebOtaLastError.length() == 0 && !Update.hasError() && Update.end(true)) {
            gWebOtaUploadSuccess = true;
            Serial.printf("[OTA-WEB] Firmware validated: %u bytes\n", (unsigned int)(index + len));
          }
          else {
            if (gWebOtaLastError.length() == 0)
              gWebOtaLastError = "Validation finale du firmware impossible";
            Update.printError(Serial);
            gWebOtaUploadSuccess = false;
          }
          gWebOtaUploadStarted = false;
        }
      });
}

#endif
