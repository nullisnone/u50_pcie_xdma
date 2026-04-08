'''
Author: Renjie Qi (qirj@yusur.tech)
date: 2024-02-02 14:13:45
'''

try:
    from cffi import FFI
except ImportError:
    print("未检测到 'cffi' 模块。请先安装 'cffi' 模块后再运行此脚本。")

class YusurNdppEnum:
    def __init__(self):
        self._members = {}

    def add_member(self, name, value):
        if name in self._members:
            raise KeyError(f"Member {name} already exists.")
        self._members[name] = value

    def __getattr__(self, name):
        if name in self._members:
            return self._members[name]
        raise AttributeError(f"No such member: {name}")

class YusurNdppImpl:
    _instance = None
    _lib = None
    _ffi = FFI()
    #_lib_path = '/usr/lib/libyusur_ndpp.so'
    _lib_path = '/usr/lib/libndpp.so'
    #_header_path = '/usr/local/include/yusur_ndpp_cpython/yusur_ndpp_define.h'
    _header_path = '/usr/local/include/ndpp_cpython/ndpp_define.h'
    _error_enum = None
    _flag_enum = None

    def __new__(cls, lib_path=None, header_path=None, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super(YusurNdppImpl, cls).__new__(cls)
            cls._lib_path = lib_path if lib_path is not None else cls._lib_path
            cls._header_path = header_path if header_path is not None else cls._header_path
            try:
                # 尝试加载C声明
                cls._load_c_declarations()
                # 尝试加载动态库，使用保存的路径
                cls._lib = cls._ffi.dlopen(cls._lib_path)
                # 取错误码和标志位的整型
                cls.convert_macros_to_enum()
            except Exception as e:
                print(f"加载动态库或C声明失败: {e}")
                raise
        return cls._instance

    @classmethod
    def _load_c_declarations(cls):
        with open(cls._header_path, "r") as f:
            c_definitions = f.read()
        cls._ffi.cdef(c_definitions)

    @classmethod
    def convert_macros_to_enum(cls):
        cls._error_enum = YusurNdppEnum()
        cls._flag_enum = YusurNdppEnum()
        cls._error_enum.add_member('NDPP_EINVAL', cls._lib.NDPP_EINVAL)
        cls._error_enum.add_member('NDPP_EAGAIN', cls._lib.NDPP_NONBLOCK)
        cls._error_enum.add_member('NDPP_EMSGSIZE', cls._lib.NDPP_EMSGSIZE)
        cls._error_enum.add_member('NDPP_EBUFINUSE', cls._lib.NDPP_EBUFINUSE)
        cls._error_enum.add_member('NDPP_ENOFEEDBACK', cls._lib.NDPP_ENOFEEDBACK)
        cls._error_enum.add_member('NDPP_ENODEV', cls._lib.NDPP_ENODEV)
        cls._error_enum.add_member('NDPP_EREGOFFSET', cls._lib.NDPP_EREGOFFSET)
        cls._error_enum.add_member('NDPP_ETRUNCATED', cls._lib.NDPP_ETRUNCATED)
        cls._error_enum.add_member('NDPP_EHUGEPAGES', cls._lib.NDPP_EHUGEPAGES)
        cls._error_enum.add_member('NDPP_ELICENSE', cls._lib.NDPP_ELICENSE)

        cls._flag_enum.add_member('NDPP_DEFAULT', 0)
        cls._flag_enum.add_member('NDPP_NONBLOCK', cls._lib.NDPP_NONBLOCK)

    def get_yusur_ndpp_enum(self):
        """
        返回 yusur_ndpp 的错误码和标志位整型定义。

        参数:
            无。

        返回值:
            error_enum(YusurNdppEnum): 错误码定义。
            flag_enum(YusurNdppEnum): 标志位定义。
        """
        return self._error_enum, self._flag_enum

    def yusur_ndpp_dev_create(self, dev_name):
        """
        根据设备名称创建 yusur_ndpp 设备实例，若创建成功，返回实例指针，否则返回 None。
    
        参数:
            dev_name (str): yusur_ndpp设备名, 通过 yusur_ctl 工具可查看。
    
        返回值:
            创建成功返回实例指针，失败返回 None。
        """
        # 1. 先打印原始传入的 dev_name（Python 字符串）
        #print(f"[DEBUG] 原始 Python 设备名: dev_name = '{dev_name}'")
    
        # 2. 转换为 C 风格字符数组
        dev_name_c = self._ffi.new("char[]", dev_name.encode('utf-8', errors='replace'))
    
        # 3. 打印 dev_name_c 的内容（核心：C字符数组 → Python字符串）
        # 方法1：用 ffi.string() 转换（推荐，自动处理 NULL 终止符）
        dev_name_c_str = self._ffi.string(dev_name_c).decode('utf-8')
       # print(f"[DEBUG] 转换后的 C 字符串: dev_name_c = '{dev_name_c_str}'")
    
        # 可选：方法2 打印 C 字符数组的内存地址（调试用）
        #print(f"[DEBUG] dev_name_c 的内存地址: {dev_name_c}")
    
        # 4. 调用 C 函数创建设备
        dev_ptr = self._lib.ndpp_dev_create(dev_name_c)
    
        if dev_ptr == self._ffi.NULL:
            print("create device failed")
            return None
        else:
            # print(f"[DEBUG] 设备创建成功，设备指针: {dev_ptr}")
            return dev_ptr


    def yusur_ndpp_dev_destroy(self, dev_ptr):
        """
        销毁 yusur_ndpp 设备实例。

        参数:
            dev_ptr: 指向 yusur_ndpp 设备实例的指针。
        """
        self._lib.ndpp_dev_destroy(dev_ptr)

    def yusur_ndpp_register_read32(self, dev_ptr, offset):
        """
        从用户自定义寄存器空间读取 uint32_t 长度的数据。

        参数:
            dev_ptr: 指向 yusur_ndpp 设备实例的指针。
            offset(int): 用户操作的寄存器偏移地址。
        
        返回:
            result(int): 成功返回 0, 失败返回 -1
            value(int): 成功时读取到的值，失败返回 None
        """
        value = self._ffi.new("uint32_t *")
        result = self._lib.ndpp_register_read32(dev_ptr, offset, value)
        if result == 0:
            return result, value[0]
        else:
            return result, None

    def yusur_ndpp_register_write32(self, dev_ptr, offset, value):
        """
        向用户自定义寄存器空间写入 uint32_t 长度的数据。

        参数:
            dev_ptr: 指向 yusur_ndpp_dev 结构体的指针。
            offset(int): 用户操作的寄存器偏移地址。
            value(int): 写入的数据。
        
        返回:
            result(int): 成功返回 0, 失败返回 -1
        """
        return self._lib.ndpp_register_write32(dev_ptr, offset, value)

    def get_current_ndpp_error_msg(self):
        """
        获取当前错误码对应的错误码和描述字符串。

        参数:
            无。

        返回:
            error_code(int): 错误码。
            error_msg(str): 错误描述的 Python 字符串。
        """
        return self._ffi.errno, self.__yusur_ndpp_strerror(self._ffi.errno)

    def __yusur_ndpp_strerror(self, error_code):
        """
        获取给定错误码对应的错误描述字符串。

        参数:
            error_code (int): 错误码。

        返回:
            error_msg(str): 错误描述的 Python 字符串。
        """
        c_str = self._lib.ndpp_strerror(error_code);
        if c_str != self._ffi.NULL:
            return self._ffi.string(c_str).decode('utf-8')
        else:
            return "获取错误信息失败"
    
    def yusur_ndpp_tx_create(self, dev_ptr, tx_id):
        """
        创建一个yusur_ndpp的发送(TX)缓冲区实例。

        参数:
            dev_ptr: 指向 YUSUR NDPP 实例的指针，代表一个已经初始化的设备实例。
            tx_id(int): 表示要创建的发送缓冲区的标识符。

        返回:
            创建成功返回 tx buffer 指针，失败返回 None
        """
        dev_tx_ptr = self._lib.ndpp_tx_create(dev_ptr, tx_id)
        if dev_tx_ptr == self._ffi.NULL:
            return None
        else:
            return dev_tx_ptr 

    def yusur_ndpp_tx_destroy(self, dev_tx_ptr):
        """
        销毁 yusur_ndpp tx buffer

        参数:
            dev_tx_ptr: tx buffer实例指针
        """
        self._lib.ndpp_tx_destroy(dev_tx_ptr)

    def yusur_ndpp_rx_create(self, dev_ptr, rx_id):
        """
        创建一个yusur_ndpp的接收(RX)缓冲区实例。

        参数:
            dev_ptr: 指向 YUSUR NDPP 实例的指针，代表一个已经初始化的设备实例。
            rx_id(int): 指定要创建的 rx buffer id。

        返回:
            创建成功返回 rx buffer 指针，失败返回 None
        """
        dev_rx_ptr = self._lib.ndpp_rx_create(dev_ptr, rx_id)
        if dev_rx_ptr == self._ffi.NULL:
            return None
        else:
            return dev_rx_ptr

    def yusur_ndpp_rx_destroy(self, dev_rx_ptr):
        """
        销毁一个 yusur_ndpp 的接收(RX)缓冲区实例。

        参数:
            dev_rx_ptr: rx buffer 实例指针。
        """
        self._lib.ndpp_rx_destroy(dev_rx_ptr)

    def yusur_ndpp_receive(self, dev_rx_ptr, buf, buf_len, flags):
        """
        从yusur_ndpp的接收(RX)缓冲区中接收数据。

        参数:
            dev_rx_ptr: rx buffer实例指针。
            buf(bytes): 接收数据的用户buffer首地址。
            buf_len(int): 接收数据的用户 buffer 长度。
            flags(int): 接收数据flags, 传入0采用默认阻塞模式。

        返回:
            result(int): 实际接收到的数据长度，失败返回 -1
        """
        cdata_buf = self._ffi.from_buffer(buf)
        return self._lib.ndpp_receive_0(dev_rx_ptr, cdata_buf, buf_len, flags)

    def yusur_ndpp_transmit(self, dev_tx_ptr, buf, buf_len, flags):
        """
        通过 tx buffer 发送数据。

        参数:
            dev_tx_ptr: tx buffer 实例指针。
            buf(bytes): 要发送的用户 buffer 首地址。
            buf_len(int): 要发送的用户 buffer 长度。
            flags(int): 发送数据 flags, 传入 0 采用默认模式。

        返回:
            result(int): 实际发送成功的数据长度，失败返回 -1
        """
        return self._lib.ndpp_transmit(dev_tx_ptr, self._ffi.from_buffer(buf), buf_len, flags)
