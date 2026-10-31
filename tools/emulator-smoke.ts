/* Headless enhanced-IIe compatibility smoke test. Not a physical IIc test. */
import {readFileSync, writeFileSync} from 'node:fs';
import {CPU6502, FLAVOR_ROCKWELL_65C02} from '@whscullin/cpu6502';
import RAM from 'emu/js/ram';
import MMU from 'emu/js/mmu';
import Apple2IO from 'emu/js/apple2io';
import DiskII from 'emu/js/cards/disk2';
import Apple2ROM from 'emu/js/roms/system/apple2enh';

(globalThis as any).window = {setTimeout,clearTimeout};
const flags: Record<string,boolean>={Text:true};
const vm:any={reset(){flags.Text=true;},page(){},
    text(v:boolean){flags.Text=v},mixed(v:boolean){flags.Mixed=v},
    hires(v:boolean){flags.Hires=v},_80col(v:boolean){flags['80Col']=v},
    altChar(v:boolean){flags.AltChar=v},doubleHires(v:boolean){flags.DoubleHires=v}};
for(const k of ['Text','Mixed','Hires','80Col','AltChar','DoubleHires'])vm['is'+k]=()=>!!flags[k];
const cpu=new CPU6502({flavor:FLAVOR_ROCKWELL_65C02});
const ram=[new RAM(0,0xbf),new RAM(0,0xbf)];
const vp:any={bank0:()=>ram[0],bank1:()=>ram[1]};
const io=new Apple2IO(cpu,vm);
const mmu=new MMU(cpu,vm,vp,vp,vp,vp,io,ram,new Apple2ROM());
cpu.addPageHandler(mmu);
const disk=new DiskII(io,{driveLight(){},dirty(){},label(){}});
io.setSlot(6,disk);
const program=process.argv[2];
if(!program)throw Error('Usage: emulator-smoke.cjs PROGRAM.dsk [DATA.dsk]');
function load(path:string){const b=readFileSync(path);return b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength);}
async function main(){
 await disk.setBinary(1,'A2CALC',program.endsWith('.po')?'po':'dsk',load(program));
 if(process.argv[3])await disk.setBinary(2,'DATA',process.argv[3].endsWith('.po')?'po':'dsk',load(process.argv[3]));
 cpu.reset();
 function run(cycles:number){for(let i=0;i<cycles;i+=20000){cpu.stepCycles(20000);mmu.resetVB();io.tick();}}
 function screen(){
  const out=[];for(let y=0;y<24;y++){
   const addr=0x400+(y&7)*0x80+((y>>3)*0x28);let row='';
   for(let x=0;x<(flags['80Col']?80:40);x++){
    const n=ram[flags['80Col']?(1-(x&1)):0].read(addr>>8,(addr&255)+(flags['80Col']?(x>>1):x));
    let c=n&127;if(c<32)c+=64;row+=String.fromCharCode(c);
   }out.push(row);
  }return out.join('\n');
 }
 for(let n=0;n<25;n++){run(2000000);let s=screen();if(s.includes('A2CALC'))break;}
 run(2000000);
 let s=screen();
 console.log('--- SCREEN AFTER BOOT ---\n'+s);
 writeFileSync('build/emulator-boot.txt',s+'\n');
 if(!s.includes('A2CALC'))throw Error('Boot did not reach A2CALC banner');
 if(!flags['80Col'])throw Error('Application did not enable 80-column text');
 console.log('PASS: bootable ProDOS disk reached application; 80-column flag='+!!flags['80Col']);
 // Commands may be supplied by the test caller as hexadecimal keyboard codes.
 const sequence=process.env.A2CALC_KEYS;
 if(sequence){for(const code of sequence.split(',').filter(Boolean)){if(code.startsWith('wait:')){run(parseInt(code.slice(5),10));continue;}io.keyDown(parseInt(code,16));for(let n=0;n<100;n++){run(100000);if(!((io as any)._key&128))break;}io.keyUp();run(2000000);if(process.env.A2CALC_TRACE)console.log('KEY '+code+'\n'+screen());}
  run(4000000);if(process.env.A2CALC_EXPECT){for(let n=0;n<75&&!screen().includes(process.env.A2CALC_EXPECT);n++)run(2000000);}s=screen();console.log('--- SCREEN AFTER INPUT ---\n'+s);writeFileSync('build/emulator-input.txt',s+'\n');
  if(process.env.A2CALC_TRACE)console.log('CPU',cpu.getState());if(process.env.A2CALC_EXPECT&&!s.includes(process.env.A2CALC_EXPECT)){console.error('Expected text absent: '+process.env.A2CALC_EXPECT);process.exitCode=1;}
 }
 if(process.env.A2CALC_DATA_OUT&&process.argv[3]){const out=await disk.getBinary(2,'dsk');if(out)writeFileSync(process.env.A2CALC_DATA_OUT,new Uint8Array(out.data));}
}
main().catch(e=>{console.error(e);process.exitCode=1});
