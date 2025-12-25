#![no_std]
#![no_main]

use core::panic::PanicInfo;

const SYS_WRITE: usize = 64;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn _start() -> ! {
    let msg = b"Hello, World!\n";
    unsafe {
        _write(1, msg.as_ptr() as *mut u8, msg.len() as i32);
    }
    loop {}
}

unsafe fn _write(fd: i32, ptr: *mut u8, len: i32) -> i32 {
    let mut ret: i32;
    unsafe {
        core::arch::asm!(
            "ecall",
            inlateout("a0") fd => ret,
            in("a1") ptr,
            in("a2") len,
            in("a7") SYS_WRITE,
            options(nostack)
        );
    }
    ret
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
