export namespace main {
	
	export class Config {
	    device: string;
	    username: string;
	    password: string;
	    autoConnect: boolean;
	    minimizeToTray: boolean;
	    autoStart: boolean;
	
	    static createFrom(source: any = {}) {
	        return new Config(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.device = source["device"];
	        this.username = source["username"];
	        this.password = source["password"];
	        this.autoConnect = source["autoConnect"];
	        this.minimizeToTray = source["minimizeToTray"];
	        this.autoStart = source["autoStart"];
	    }
	}
	export class Device {
	    id: string;
	    desc: string;
	    mac: string;
	    ip: string;
	
	    static createFrom(source: any = {}) {
	        return new Device(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.id = source["id"];
	        this.desc = source["desc"];
	        this.mac = source["mac"];
	        this.ip = source["ip"];
	    }
	}

}

