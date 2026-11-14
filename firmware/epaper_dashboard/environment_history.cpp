#include "app.h"
#include "environment_history.h"
#include <Preferences.h>

EnvironmentHistory environmentHistory;
const char* environmentHistoryError="";
uint32_t environmentHistoryRevision=0;
static bool historyDirty=false;
static void historyError(const char* error) {
  if(strcmp(environmentHistoryError,error)) {
    environmentHistoryError=error;++environmentHistoryRevision;needRefresh=true;
    Serial.printf("ENV_HISTORY_STORAGE error=%s\n",error[0]?error:"ok");
  }
}
void initEnvironmentHistory() {
  Preferences prefs;
  if(!prefs.begin("envhistory",false)) {historyError("history_open_failed");return;}
  if(prefs.isKey("samples")) {
    uint8_t bytes[ENV_HISTORY_BYTES];
    if(prefs.getBytesLength("samples")!=sizeof(bytes) || prefs.getBytes("samples",bytes,sizeof(bytes))!=sizeof(bytes) ||
       !decodeHistory(bytes,sizeof(bytes),environmentHistory))historyError("history_invalid");
  }
  prefs.end();
}
void recordEnvironmentHistory(int64_t epoch,const EnvironmentReading& reading) {
  if(!EnvironmentHistory::bucket(epoch))return;
  if(environmentHistory.record(epoch,reading)) {historyDirty=true;++environmentHistoryRevision;needRefresh=true;}
  if(!historyDirty)return;
  Preferences prefs;
  if(!prefs.begin("envhistory",false)) {historyError("history_open_failed");return;}
  uint8_t bytes[ENV_HISTORY_BYTES];encodeHistory(environmentHistory,bytes);
  bool saved=prefs.putBytes("samples",bytes,sizeof(bytes))==sizeof(bytes);prefs.end();
  if(saved) {historyDirty=false;historyError("");}
  else historyError("history_write_failed"); // Retry at the next sensor minute, not every loop.
  Serial.printf("ENV_HISTORY epoch=%llu points=%d saved=%d error=%s\n",static_cast<unsigned long long>(epoch),environmentHistory.count(epoch),saved,environmentHistoryError);
}
void environmentHistoryJson(JsonDocument& doc,bool samples) {
  const int64_t now=clockReady()?time(nullptr):0;const auto* latest=environmentHistory.latest(now);
  auto h=doc["environmentHistory"].to<JsonObject>();
  h["intervalSeconds"]=ENV_HISTORY_INTERVAL;h["capacity"]=ENV_HISTORY_CAPACITY;
  h["count"]=environmentHistory.count(now);h["latestRecordedAt"]=latest?latest->epoch:0;
  h["storageError"]=environmentHistoryError;h["pendingSave"]=historyDirty;
  if(samples) {
    auto points=h["points"].to<JsonArray>();const uint32_t end=EnvironmentHistory::bucket(now);
    if(end)for(int i=ENV_HISTORY_CAPACITY-1;i>=0;--i)if(const auto* p=environmentHistory.at(end-uint32_t(i))) {
      if(p->epoch>now)continue;
      auto row=points.add<JsonArray>();row.add(p->epoch);
      if(p->hasTemperature())row.add(p->temperature/10.0f);else row.add(nullptr);
      if(p->hasHumidity())row.add(p->humidity/10.0f);else row.add(nullptr);
    }
  }
}
