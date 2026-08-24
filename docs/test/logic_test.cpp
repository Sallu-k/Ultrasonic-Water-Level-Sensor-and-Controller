#include <cstdio>
#include <cstdint>
const float SENSOR_TO_EMPTY=14.0, SENSOR_TO_FULL=3.0;
const uint8_t PUMP_ON_BELOW=10, PUMP_OFF_ABOVE=90;
bool pumpRunning=false, overrideOn=false, sensorFailed=false;

int distanceToPercent(float d){
  const float span=SENSOR_TO_EMPTY-SENSOR_TO_FULL;
  if(span<=0.0) return 0;
  float p=((SENSOR_TO_EMPTY-d)/span)*100.0;
  if (p <   0.0) p =   0.0;
  if (p > 100.0) p = 100.0;
  return (int)(p+0.5);
}
void updatePump(int lv){
  if(sensorFailed){pumpRunning=false;return;}
  if(lv>=PUMP_OFF_ABOVE){pumpRunning=false;overrideOn=false;return;}
  if(overrideOn){pumpRunning=true;return;}
  if(lv<PUMP_ON_BELOW){pumpRunning=true;}
}
int fails=0;
void chk(const char*n,bool c){printf("%-52s %s\n",n,c?"PASS":"** FAIL **"); if(!c)fails++;}

int main(){
 printf("--- distanceToPercent ---\n");
 chk("empty (14.0cm) -> 0%", distanceToPercent(14.0)==0);
 chk("full  (3.0cm)  -> 100%", distanceToPercent(3.0)==100);
 chk("midpoint (8.5cm) -> 50%", distanceToPercent(8.5)==50);
 chk("clamp low: 20cm (below empty) -> 0%", distanceToPercent(20.0)==0);
 chk("clamp high: 1cm (above full) -> 100%", distanceToPercent(1.0)==100);
 chk("inverted: 5cm > 12cm in percent", distanceToPercent(5.0)>distanceToPercent(12.0));

 printf("\n--- hysteresis: slow fill then drain ---\n");
 pumpRunning=false; overrideOn=false; sensorFailed=false;
 int onAt=-1,offAt=-1,transitions=0; bool prev=false;
 for(int lv=0;lv<=100;lv++){ updatePump(lv);
   if(pumpRunning!=prev){transitions++; if(pumpRunning)onAt=lv; else offAt=lv; prev=pumpRunning;} }
 chk("pump turns ON at 0% (starts empty)", onAt==0);
 chk("pump turns OFF at exactly 90%", offAt==90);
 for(int lv=100;lv>=0;lv--){ updatePump(lv);
   if(pumpRunning!=prev){transitions++; prev=pumpRunning; if(pumpRunning) onAt=lv;} }
 chk("on the way down, ON again at 9% (not 90%)", onAt==9);
 chk("only 3 transitions across a full cycle", transitions==3);

 printf("\n--- chatter test: jitter around a single point ---\n");
 pumpRunning=false; overrideOn=false;
 for(int lv=0;lv<5;lv++) updatePump(0);         // prime: pump on
 bool wasOn=pumpRunning; int flips=0;
 int noisy[]={49,51,50,52,48,51,49,50,53,47};   // ripple around 50%
 for(int i=0;i<10;i++){ updatePump(noisy[i]); if(pumpRunning!=wasOn){flips++;wasOn=pumpRunning;} }
 chk("no switching while jittering mid-band", flips==0);
 chk("pump held its previous state (ON)", pumpRunning==true);

 printf("\n--- override ---\n");
 pumpRunning=false; overrideOn=true; sensorFailed=false;
 updatePump(50); chk("override starts pump mid-band", pumpRunning==true);
 updatePump(95); chk("90%% cutoff beats override", pumpRunning==false);
 chk("override auto-cancels when full", overrideOn==false);

 printf("\n--- sensor fault ---\n");
 pumpRunning=true; overrideOn=true; sensorFailed=true;
 updatePump(5); chk("fault stops pump even with override on", pumpRunning==false);

 printf("\n%s (%d failures)\n", fails? "FAILURES PRESENT":"ALL TESTS PASSED", fails);
 return fails;
}
