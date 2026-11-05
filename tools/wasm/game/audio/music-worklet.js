import {MusicMixer} from './music-mixer.js';
// AudioWorklet has no wall-clock API; decoder profiling only needs monotonic time.
globalThis.performance ||= {now:()=>currentTime*1000};
globalThis.TextDecoder ||= class {
 decode(bytes){let text='';for(let i=0;i<bytes.length;){let c=bytes[i++];if(c>=0xc0&&c<0xe0)c=((c&31)<<6)|(bytes[i++]&63);else if(c>=0xe0&&c<0xf0)c=((c&15)<<12)|((bytes[i++]&63)<<6)|(bytes[i++]&63);else if(c>=0xf0)c=((c&7)<<18)|((bytes[i++]&63)<<12)|((bytes[i++]&63)<<6)|(bytes[i++]&63);text+=String.fromCodePoint(c)}return text}
};
// The official runtime does not require a browser TextEncoder for its own C API.
globalThis.TextEncoder ||= class {encode(s){return Uint8Array.from(s,c=>c.charCodeAt(0))}};
class PvzMusicProcessor extends AudioWorkletProcessor {
 constructor(options){
  super();this.mixer=null;this.reportFrames=0;this.failed=false;
  this.port.onmessage=event=>{try{
   if(event.data.op==='flush')this.port.postMessage({flush:event.data.token,voices:this.mixer.positions()});
   else this.mixer.command(event.data);
  }catch(error){this.fail(error)}};
  MusicMixer.create(options.processorOptions.compiled,sampleRate,options.processorOptions.entropy).then(mixer=>{this.mixer=mixer;this.port.postMessage({ready:true})}).catch(error=>this.fail(error));
 }
 fail(error){this.failed=true;this.port.postMessage({error:String(error.stack||error)})}
 process(inputs,outputs){
  const channels=outputs[0];if(!channels?.length||this.failed)return true;
  try{
   if(this.mixer){this.mixer.render(channels[0],channels[1]);this.reportFrames+=channels[0].length;
    if(this.reportFrames>=1024){this.reportFrames=0;this.port.postMessage({voices:this.mixer.positions()})}}
  }catch(error){this.fail(error)}
  return true;
 }
}
registerProcessor('pvz-music',PvzMusicProcessor);
