#include <vex/limits.h>
#include <vex/signal.h>
#include <vex/termios.h>
#include <vex/tty_ldisc.h>

#define TAB_WIDTH 8
#define DELETE    0x7F
#define BELL      0x07

#define CONTROL_FLIP 0x40 // Turns a control byte into the letter echoed after ^, and back

static bool is_control(uint8_t byte)
{
    return byte < 0x20 || byte == DELETE;
}

static bool is_upper(uint8_t byte)
{
    return byte >= 'A' && byte <= 'Z';
}

static bool is_lower(uint8_t byte)
{
    return byte >= 'a' && byte <= 'z';
}

static bool is_blank(uint8_t byte)
{
    return byte == ' ' || byte == '\t';
}

static bool is_utf8_continuation(uint8_t byte)
{
    return (byte & 0xC0) == 0x80;
}

static bool is_special(const termios_t* restrict termios, uint8_t byte, unsigned index)
{
    uint8_t special = termios->c_cc[index];
    return special != _POSIX_VDISABLE && special == byte;
}

// XCASE stands a backslash and one of these for a character an uppercase-only terminal lacks
static uint8_t xcase_escape(uint8_t byte)
{
    switch(byte) {
        case '{':
            return '(';
        case '}':
            return ')';
        case '|':
            return '!';
        case '~':
            return '^';
        case '`':
            return '\'';
        default:
            return 0;
    }
}

static uint8_t xcase_unescape(uint8_t byte)
{
    switch(byte) {
        case '(':
            return '{';
        case ')':
            return '}';
        case '!':
            return '|';
        case '^':
            return '~';
        case '\'':
            return '`';
        default:
            return 0;
    }
}

// Fill bytes stand in for the delays; at 110 baud each one takes a tenth of a second.
// A delay field over its lowest bit, such as CRDLY over CR1, is the delay's number
static unsigned delay_fills(uint32_t oflag, uint8_t byte)
{
    switch(byte) {
        case '\n':
            return (oflag & NLDLY) / NL1;
        case '\r':
            return (oflag & CRDLY) / CR1;
        case '\t':
            return (oflag & TABDLY) / TAB1;
        case '\b':
            return (oflag & BSDLY) / BS1;
        case '\v':
            return 2 * ((oflag & VTDLY) / VT1);
        case '\f':
            return 2 * ((oflag & FFDLY) / FF1);
        default:
            return 0;
    }
}

static uint32_t next_tab_stop(uint32_t column)
{
    return (column / TAB_WIDTH + 1) * TAB_WIDTH;
}

// Columns a byte of the line took when it was echoed
static unsigned echo_width(const termios_t* restrict termios, uint8_t byte)
{
    if(is_control(byte)) {
        if(termios->c_lflag & ECHOCTL) {
            return 2;
        }

        return 0;
    }

    if((termios->c_iflag & IUTF8) && is_utf8_continuation(byte)) {
        return 0;
    }

    return 1;
}

static size_t expand_printable(const termios_t* restrict termios, uint8_t byte, uint8_t* restrict out, uint32_t* restrict column)
{
    size_t length = 0;

    if((termios->c_lflag & (XCASE | ICANON)) == (XCASE | ICANON)) {
        uint8_t escape = xcase_escape(byte);

        if(is_upper(byte)) {
            out[length] = '\\';
            length += 1;
        }
        else if(escape) {
            out[length] = '\\';
            length += 1;
            byte = escape;
        }
    }

    if((termios->c_oflag & OLCUC) && is_lower(byte)) {
        byte -= 'a' - 'A';
    }

    out[length] = byte;
    length += 1;

    if(!is_control(byte) && !((termios->c_iflag & IUTF8) && is_utf8_continuation(byte))) {
        *column += length;
    }

    return length;
}

static size_t expand(const termios_t* restrict termios, uint8_t byte, uint8_t* restrict out, uint32_t* restrict column)
{
    uint32_t oflag = termios->c_oflag;

    if(0 == (oflag & OPOST)) {
        out[0] = byte;

        if(echo_width(termios, byte) == 1) {
            *column += 1;
        }

        return 1;
    }

    size_t length = 0;

    if(byte == '\n') {
        if(oflag & ONLCR) {
            out[length] = '\r';
            length += 1;
            *column = 0;
        }
        else if(oflag & ONLRET) {
            *column = 0;
        }

        out[length] = '\n';
        length += 1;
    }
    else if(byte == '\r') {
        if((oflag & ONOCR) && *column == 0) {
            return 0;
        }

        if(oflag & OCRNL) {
            byte = '\n';

            if(oflag & ONLRET) {
                *column = 0;
            }
        }
        else {
            *column = 0;
        }

        out[length] = byte;
        length += 1;
    }
    else if(byte == '\t') {
        uint32_t stop = next_tab_stop(*column);

        if((oflag & TABDLY) == TAB3) {
            while(*column < stop) {
                out[length] = ' ';
                length += 1;
                *column += 1;
            }

            return length;
        }

        *column = stop;
        out[length] = '\t';
        length += 1;
    }
    else if(byte == '\b') {
        if(*column > 0) {
            *column -= 1;
        }

        out[length] = '\b';
        length += 1;
    }
    else if(byte == '\v' || byte == '\f') {
        out[length] = byte;
        length += 1;
    }
    else {
        return expand_printable(termios, byte, out, column);
    }

    if(oflag & OFILL) {
        uint8_t fill = 0;

        if(oflag & OFDEL) {
            fill = DELETE;
        }

        for(unsigned i = delay_fills(oflag, byte); i > 0; --i) {
            out[length] = fill;
            length += 1;
        }
    }

    return length;
}

static void output_or_drop(tty_device_t* restrict device, uint8_t byte)
{
    // Echo only: the user sees less, but the input is kept
    (void)tty_ldisc_output(device, byte);
}

static void echo(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;

    if((termios->c_lflag & ECHOCTL) && is_control(byte) && byte != '\t' && byte != '\n') {
        output_or_drop(device, '^');
        output_or_drop(device, byte ^ CONTROL_FLIP);
        return;
    }

    output_or_drop(device, byte);
}

static void echo_string(tty_device_t* restrict device, const char* restrict string)
{
    while(*string) {
        output_or_drop(device, *string);
        string += 1;
    }
}

static bool delimiter_at(const tty_ldisc_state_t* restrict ldisc, uint32_t position)
{
    uint32_t bit = position % TTY_RING_SIZE;
    return ldisc->delimiters[bit / 8] & (1U << (bit % 8));
}

static void store(tty_device_t* restrict device, uint8_t byte, bool delimiter)
{
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    tty_ring_t* input = &device->priv.input;
    uint32_t bit = input->tail % TTY_RING_SIZE;

    if(delimiter) {
        ldisc->delimiters[bit / 8] |= 1U << (bit % 8);
    }
    else {
        ldisc->delimiters[bit / 8] &= ~(1U << (bit % 8));
    }

    tty_ring_put(input, byte);
}

static void close_hardcopy_erase(tty_device_t* restrict device)
{
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;

    if(ldisc->hardcopy_erasing) {
        ldisc->hardcopy_erasing = false;
        echo(device, '/');
    }
}

static uint32_t line_width(const tty_device_t* restrict device, uint32_t end)
{
    const termios_t* termios = &device->priv.termios;
    const tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    uint32_t column = ldisc->line_column;

    for(uint32_t position = ldisc->line_end; position != end; position += 1) {
        uint8_t byte = tty_ring_at(&device->priv.input, position);

        if(byte == '\t') {
            column = next_tab_stop(column);
        }
        else {
            column += echo_width(termios, byte);
        }
    }

    return column;
}

static void echo_erase(tty_device_t* restrict device, uint32_t erased_at, uint32_t erased_end)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;

    if(termios->c_lflag & ECHOPRT) {
        if(!ldisc->hardcopy_erasing) {
            ldisc->hardcopy_erasing = true;
            echo(device, '\\');
        }

        for(uint32_t position = erased_at; position != erased_end; position += 1) {
            echo(device, tty_ring_at(&device->priv.input, position));
        }

        return;
    }

    uint8_t lead = tty_ring_at(&device->priv.input, erased_at);

    if(lead == '\t') {
        uint32_t target = line_width(device, erased_at);

        for(uint32_t i = ldisc->column - target; i > 0 && ldisc->column > target; --i) {
            output_or_drop(device, '\b');
        }

        return;
    }

    for(unsigned i = echo_width(termios, lead); i > 0; --i) {
        echo_string(device, "\b \b");
    }
}

// The bytes stay in the ring past the
// tail so the echo can still look at them
static bool erase_last(tty_device_t* restrict device)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    tty_ring_t* input = &device->priv.input;
    uint32_t end = input->tail;

    if(input->tail == ldisc->line_end) {
        return false;
    }

    input->tail -= 1;

    while((termios->c_iflag & IUTF8) && input->tail != ldisc->line_end && is_utf8_continuation(tty_ring_at(input, input->tail))) {
        input->tail -= 1;
    }

    if((termios->c_lflag & (ECHO | ECHOE)) == (ECHO | ECHOE) || (termios->c_lflag & (ECHO | ECHOPRT)) == (ECHO | ECHOPRT)) {
        echo_erase(device, input->tail, end);
    }

    return true;
}

static bool erases_visibly(const termios_t* restrict termios)
{
    return 0 != (termios->c_lflag & (ECHOE | ECHOPRT));
}

static void erase(tty_device_t* restrict device, uint8_t byte)
{
    if(!erase_last(device)) {
        return;
    }

    const termios_t* termios = &device->priv.termios;

    if((termios->c_lflag & ECHO) && !erases_visibly(termios)) {
        echo(device, byte);
    }
}

static void erase_word(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    const tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    tty_ring_t* input = &device->priv.input;

    while(input->tail != ldisc->line_end && is_blank(tty_ring_at(input, input->tail - 1))) {
        (void)erase_last(device);
    }

    while(input->tail != ldisc->line_end && !is_blank(tty_ring_at(input, input->tail - 1))) {
        (void)erase_last(device);
    }

    if((termios->c_lflag & ECHO) && !erases_visibly(termios)) {
        echo(device, byte);
    }
}

static void kill_line(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    uint32_t lflag = termios->c_lflag;

    if((lflag & (ECHO | ECHOKE | ECHOE)) == (ECHO | ECHOKE | ECHOE)) {
        while(erase_last(device)) {
            // empty
        }

        return;
    }

    device->priv.input.tail = ldisc->line_end;

    if(lflag & ECHO) {
        echo(device, byte);

        if(lflag & ECHOK) {
            echo(device, '\n');
        }
    }

    ldisc->line_column = ldisc->column;
}

static void reprint(tty_device_t* restrict device, uint8_t byte)
{
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    const tty_ring_t* input = &device->priv.input;

    if(0 == (device->priv.termios.c_lflag & ECHO)) {
        return;
    }

    echo(device, byte);
    echo(device, '\n');

    ldisc->line_column = ldisc->column;

    for(uint32_t position = ldisc->line_end; position != input->tail; position += 1) {
        echo(device, tty_ring_at(input, position));
    }
}

static void put_data(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    const tty_ring_t* input = &device->priv.input;
    bool canonical = termios->c_lflag & ICANON;

    close_hardcopy_erase(device);

    // A canonical line keeps a byte free for the delimiter that ends it
    if(canonical && (tty_ring_room(input) <= 1 || input->tail - ldisc->line_end >= MAX_CANON - 1)) {
        if(termios->c_iflag & IMAXBEL) {
            output_or_drop(device, BELL);
        }

        return;
    }

    if(tty_ring_room(input) == 0) {
        return;
    }

    if(termios->c_lflag & ECHO) {
        echo(device, byte);
    }

    store(device, byte, false);

    if(!canonical) {
        ldisc->line_end = input->tail;
    }
}

// A NUL delimiter is VEOF: no special character can be NUL, which is _POSIX_VDISABLE
static void end_line(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;

    close_hardcopy_erase(device);

    if(tty_ring_room(&device->priv.input) == 0) {
        return;
    }

    if(byte && ((termios->c_lflag & ECHO) || (byte == '\n' && (termios->c_lflag & ECHONL)))) {
        echo(device, byte);
    }

    store(device, byte, true);

    ldisc->line_end = device->priv.input.tail;
    ldisc->line_column = ldisc->column;
}

static bool flow_control(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;

    if(0 == (termios->c_iflag & IXON)) {
        return false;
    }

    bool start = is_special(termios, byte, VSTART);
    bool stop = is_special(termios, byte, VSTOP);

    if(start && stop) {
        ldisc->output_stopped = !ldisc->output_stopped;
        return true;
    }

    if(stop) {
        ldisc->output_stopped = true;
        return true;
    }

    if(start) {
        ldisc->output_stopped = false;
        return true;
    }

    if(ldisc->output_stopped && (termios->c_iflag & IXANY)) {
        ldisc->output_stopped = false;
    }

    return false;
}

static int signal_of(const termios_t* restrict termios, uint8_t byte)
{
    if(is_special(termios, byte, VINTR)) {
        return SIGINT;
    }

    if(is_special(termios, byte, VQUIT)) {
        return SIGQUIT;
    }

    if(is_special(termios, byte, VSUSP)) {
        return SIGTSTP;
    }

    if(is_special(termios, byte, VSTATUS)) {
        return SIGINFO;
    }

    return 0;
}

static bool signal_character(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;

    if(0 == (termios->c_lflag & ISIG)) {
        return false;
    }

    int sig = signal_of(termios, byte);

    if(sig == 0) {
        return false;
    }

    // A status request interrupts nothing, so it keeps what was typed and is still to print
    if(sig != SIGINFO && 0 == (termios->c_lflag & NOFLSH)) {
        tty_ldisc_flush_input(device);
        device->priv.output.head = device->priv.output.tail;
    }

    if(termios->c_lflag & ECHO) {
        echo(device, byte);
    }

    ldisc->signals |= 1U << sig;

    return true;
}

static void receive_canonical(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    uint32_t lflag = termios->c_lflag;

    if(ldisc->escaped) {
        ldisc->escaped = false;

        uint8_t unescaped = xcase_unescape(byte);

        if(is_lower(byte)) {
            byte -= 'a' - 'A';
        }
        else if(unescaped) {
            byte = unescaped;
        }

        put_data(device, byte);
        return;
    }

    if(is_special(termios, byte, VERASE)) {
        erase(device, byte);
        return;
    }

    if(is_special(termios, byte, VKILL)) {
        kill_line(device, byte);
        return;
    }

    if(lflag & IEXTEN) {
        if(is_special(termios, byte, VWERASE)) {
            erase_word(device, byte);
            return;
        }

        if(is_special(termios, byte, VREPRINT)) {
            reprint(device, byte);
            return;
        }

        if(is_special(termios, byte, VLNEXT)) {
            ldisc->literal_next = true;
            return;
        }
    }

    if(is_special(termios, byte, VEOF)) {
        end_line(device, 0);
        return;
    }

    if(byte == '\n' || is_special(termios, byte, VEOL) || ((lflag & IEXTEN) && is_special(termios, byte, VEOL2))) {
        end_line(device, byte);
        return;
    }

    if((lflag & XCASE) && byte == '\\') {
        ldisc->escaped = true;

        if(lflag & ECHO) {
            echo(device, byte);
        }

        return;
    }

    put_data(device, byte);
}

static void receive_byte(tty_device_t* restrict device, uint8_t byte)
{
    const termios_t* termios = &device->priv.termios;
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    uint32_t iflag = termios->c_iflag;

    if(iflag & ISTRIP) {
        byte &= 0x7F;
    }

    if(ldisc->literal_next) {
        ldisc->literal_next = false;
        put_data(device, byte);
        return;
    }

    if(flow_control(device, byte) || signal_character(device, byte)) {
        return;
    }

    if(byte == '\r') {
        if(iflag & IGNCR) {
            return;
        }

        if(iflag & ICRNL) {
            byte = '\n';
        }
    }
    else if(byte == '\n' && (iflag & INLCR)) {
        byte = '\r';
    }

    if((iflag & IUCLC) && is_upper(byte)) {
        byte += 'a' - 'A';
    }

    if(0 == (termios->c_lflag & ICANON)) {
        put_data(device, byte);
        return;
    }

    receive_canonical(device, byte);
}

void tty_ldisc_flush_input(tty_device_t* restrict device)
{
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    tty_ring_t* input = &device->priv.input;

    input->head = input->tail;
    ldisc->line_end = input->tail;
    ldisc->line_column = ldisc->column;
    ldisc->literal_next = false;
    ldisc->escaped = false;
    ldisc->hardcopy_erasing = false;
}

void tty_ldisc_receive(tty_device_t* restrict device)
{
    tty_ring_t* flip = &device->priv.flip;

    while(tty_ring_used(flip)) {
        receive_byte(device, tty_ring_take(flip));
    }
}

bool tty_ldisc_output(tty_device_t* restrict device, uint8_t byte)
{
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    uint8_t expanded[TTY_EXPANSION_MAX];
    uint32_t column = ldisc->column;
    size_t length = expand(&device->priv.termios, byte, expanded, &column);

    if(tty_ring_room(&device->priv.output) < length) {
        return false;
    }

    for(size_t i = 0; i < length; ++i) {
        tty_ring_put(&device->priv.output, expanded[i]);
    }

    ldisc->column = column;

    return true;
}

size_t tty_ldisc_readable(const tty_device_t* restrict device)
{
    return device->priv.ldisc.line_end - device->priv.input.head;
}

size_t tty_ldisc_read(tty_device_t* restrict device, uint8_t* restrict buffer, size_t size, bool* restrict end_of_file)
{
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;
    tty_ring_t* input = &device->priv.input;
    bool canonical = device->priv.termios.c_lflag & ICANON;
    size_t copied = 0;

    *end_of_file = false;

    while(copied < size && input->head != ldisc->line_end) {
        bool delimiter = canonical && delimiter_at(ldisc, input->head);
        uint8_t byte = tty_ring_take(input);

        // VEOF ends the line without being part of
        // it, and on an empty line it is end of file
        if(delimiter && byte == 0) {
            *end_of_file = (copied == 0);
            break;
        }

        buffer[copied] = byte;
        copied += 1;

        if(delimiter) {
            break;
        }
    }

    return copied;
}

void tty_ldisc_set_termios(tty_device_t* restrict device, const termios_t* restrict old_termios)
{
    tty_ldisc_state_t* ldisc = &device->priv.ldisc;

    if((old_termios->c_lflag & ICANON) && 0 == (device->priv.termios.c_lflag & ICANON)) {
        ldisc->line_end = device->priv.input.tail;
        ldisc->literal_next = false;
        ldisc->escaped = false;
    }
}
