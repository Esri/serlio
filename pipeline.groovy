import groovy.transform.Field
import com.esri.zrh.jenkins.PipelineSupportLibrary 
import com.esri.zrh.jenkins.PslFactory
import com.esri.zrh.jenkins.psl.UploadTrackingPsl
import com.esri.zrh.jenkins.JenkinsTools
import com.esri.zrh.jenkins.ce.CityEnginePipelineLibrary
import com.esri.zrh.jenkins.ce.PrtAppPipelineLibrary

@Field def psl = PslFactory.create(this, UploadTrackingPsl.ID)
@Field def cepl = new CityEnginePipelineLibrary(this, psl)
@Field def papl = new PrtAppPipelineLibrary(cepl)


// -- GLOBAL DEFINITIONS

@Field final String REPO         = 'git@github.com:ArcGIS/serlio.git'
@Field final String REPO_CREDS   = 'jenkins-github-serlio-deployer-key'
@Field final String SOURCES      = "serlio.git/src"
@Field final String BUILD_TARGET = 'package'

// TODO: abusing grp field to distinguish maya versions per task
@Field final List CONFIGS = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC63, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2018', maya: PrtAppPipelineLibrary.Dependencies.MAYA2018 ],
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC63, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2019', maya: PrtAppPipelineLibrary.Dependencies.MAYA2019 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC141, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2018', maya: PrtAppPipelineLibrary.Dependencies.MAYA2018 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC141, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2019', maya: PrtAppPipelineLibrary.Dependencies.MAYA2019 ],
]

// -- PIPELINE

@Field String myBranch = env.BRANCH_NAME

// entry point for standalone pipeline
def pipeline(String branchName = null) {
	cepl.runParallel(getTasks(branchName))
}

// entry point for embedded pipeline
Map getTasks(String branchName = null) {
	if (branchName) myBranch = branchName

	Map tasks = [:]
	tasks << taskGenSerlio()
	return tasks
}


// -- TASK GENERATORS

Map taskGenSerlio() {
	return cepl.generateTasks('serlio', this.&taskBuildSerlio, CONFIGS)
}


// -- TASK BUILDERS

def taskBuildSerlio(cfg) {
	final String appName = 'serlio'
	final List DEPS = [ PrtAppPipelineLibrary.Dependencies.CESDK, cfg.maya ]
	List defs = [
		[ key: 'prt_DIR',           val: PrtAppPipelineLibrary.Dependencies.CESDK.p ],
		[ key: 'maya_DIR',          val: cfg.maya.p ],
		[ key: 'SRL_VERSION_BUILD', val: env.BUILD_NUMBER ]
	]
	papl.buildConfig(REPO, myBranch, SOURCES, BUILD_TARGET, cfg, DEPS, defs, REPO_CREDS)
	// note: source path will change once we move serlio into a separate repo
	
	def buildProps = papl.jpe.readProperties(file: 'build/deployment.properties')
	final String artifactPattern = "${buildProps.package_file}.*"
	final def artifactVersion = { p -> buildProps.package_version_base }

	// TODO: abusing grp again to label artifact
	def classifierExtractor = { p ->
		return cepl.getArchiveClassifier(cfg) + '-' + cfg.grp
	}
	papl.publish(appName, myBranch, artifactPattern, artifactVersion, cfg, classifierExtractor)
}


// -- make embeddable

return this
