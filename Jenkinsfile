#!/usr/bin/env groovy

// This pipeline is designed to run on Esri-internal CI infrastructure.

// -- PIPELINE LIBRARIES

@Library('psl')
import com.esri.zrh.jenkins.PipelineSupportLibrary 
import com.esri.zrh.jenkins.PslFactory 
import com.esri.zrh.jenkins.psl.UploadTrackingPsl
import com.esri.zrh.jenkins.JenkinsTools
import com.esri.zrh.jenkins.ToolInfo
import com.esri.zrh.jenkins.ce.CityEnginePipelineLibrary
import com.esri.zrh.jenkins.ce.PrtAppPipelineLibrary
import groovy.transform.Field

@Field def psl = PslFactory.create(this, UploadTrackingPsl.ID)
@Field def cepl = new CityEnginePipelineLibrary(this, psl)
@Field def papl = new PrtAppPipelineLibrary(cepl)

// -- SETUP

psl.runsHere('production')
env.PIPELINE_ARCHIVING_ALLOWED = "true"
properties([ disableConcurrentBuilds() ])

// -- GLOBAL DEFINITIONS

@Field final String REPO         = 'git@github.com:esri/serlio.git'
@Field final String REPO_CREDS   = 'jenkins-github-serlio-deployer-key'
@Field final String SOURCES      = "serlio.git/src"
@Field final String BUILD_TARGET = 'package'
@Field final String SOURCE_STASH = 'serlio-src'

@Field final List CHECKOUT_CONFIG = [ [ ba: psl.BA_CHECKOUT ] ]
// TODO: abusing grp field to distinguish maya versions per task
@Field final List CONFIGS = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93,  cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2018', maya: PrtAppPipelineLibrary.Dependencies.MAYA2018 ],
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93,  cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2019', maya: PrtAppPipelineLibrary.Dependencies.MAYA2019 ],
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93,  cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2020', maya: PrtAppPipelineLibrary.Dependencies.MAYA2020 ],
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93,  cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2022', maya: PrtAppPipelineLibrary.Dependencies.MAYA2022 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2018', maya: PrtAppPipelineLibrary.Dependencies.MAYA2018 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2019', maya: PrtAppPipelineLibrary.Dependencies.MAYA2019 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2020', maya: PrtAppPipelineLibrary.Dependencies.MAYA2020 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, grp: 'maya2022', maya: PrtAppPipelineLibrary.Dependencies.MAYA2022 ],
]

@Field final List TEST_CONFIGS = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93,  cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, maya: PrtAppPipelineLibrary.Dependencies.MAYA2022 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, maya: PrtAppPipelineLibrary.Dependencies.MAYA2022 ],
]

// -- PIPELINE

stage('prepare'){
	cepl.runParallel(getCheckoutTask())
}

stage('test') {
	cepl.runParallel(getTestTasks())
}

stage('build') {
	cepl.runParallel(getBuildTasks())
}

stage('installers') {
	cepl.runParallel(getInstallersTasks())
}

papl.finalizeRun('serlio', env.BRANCH_NAME)

// -- TASK GENERATORS

Map getCheckoutTask(){
	Map tasks = [:]
	tasks << taskGenSerlioSourceCheckout()
	return tasks
}

Map getTestTasks() {
	Map tasks = [:]
	tasks << taskGenSerlioTests()
	return tasks
}

Map getBuildTasks() {
	Map tasks = [:]
	tasks << taskGenSerlio()
	return tasks
}

Map getInstallersTasks() {
	Map tasks = [:]
	tasks << taskGenSerlioInstallers()
	return tasks
}

Map taskGenSerlioSourceCheckout() {
	return cepl.generateTasks('srl-src', this.&taskSourceCheckout, CHECKOUT_CONFIG)
}

Map taskGenSerlio() {
	return cepl.generateTasks('srl-bld', this.&taskBuildSerlio, CONFIGS)
}

Map taskGenSerlioTests() {
	return cepl.generateTasks('srl-tst', this.&taskBuildSerlioTests, TEST_CONFIGS)
}

Map taskGenSerlioInstallers() {
	return cepl.generateTasks('srl-msi', this.&taskBuildSerlioInstaller, CONFIGS.findAll { it.os == cepl.CFG_OS_WIN10})
}

// -- TASK BUILDERS

def taskSourceCheckout(cfg) {
	cepl.cleanCurrentDir()
	papl.checkout(REPO, env.BRANCH_NAME, REPO_CREDS)
	stash(name: SOURCE_STASH)
}

def taskBuildCMake(cfg, target){
	final List deps = [ cfg.maya ]
	List defs = [
		[ key: 'maya_DIR',          val: cfg.maya.p ],
		[ key: 'SRL_VERSION_BUILD', val: env.BUILD_NUMBER ]
	]
	cepl.cleanCurrentDir()
	unstash(name: SOURCE_STASH)
	deps.each { d -> papl.fetchDependency(d, cfg) }
	papl.jpe.dir(path: 'build') {
		papl.runCMakeBuild(SOURCES, target, cfg, defs)
	}
}

def taskBuildSerlio(cfg) {
	final String appName = 'serlio'

	taskBuildCMake(cfg, BUILD_TARGET)
	def buildProps = papl.jpe.readProperties(file: 'build/deployment.properties')
	final String artifactPattern = "${buildProps.package_file}.*"
	final def artifactVersion = { p -> buildProps.package_version_base }

	// TODO: abusing grp again to label artifact
	def classifierExtractor = { p ->
		return cepl.getArchiveClassifier(cfg) + '-' + cfg.grp
	}
	papl.publish(appName, env.BRANCH_NAME, artifactPattern, artifactVersion, cfg, classifierExtractor)
}

def taskBuildSerlioTests(cfg) {
	final String appName = 'serlio-test'
	taskBuildCMake(cfg, 'build_and_run_tests')
	papl.jpe.junit('build/test/serlio_test_report.xml')
}

def taskBuildSerlioInstaller(cfg) {
	final String appName = 'serlio-installer'
	cepl.cleanCurrentDir()
	unstash(name: SOURCE_STASH)
	final List deps = [ cfg.maya ]
	deps.each { d -> papl.fetchDependency(d, cfg) }

	// Toolchain definition for building MSI installers.
	final JenkinsTools compiler = cepl.getToolchainTool(cfg)
	final def toolchain = [
		new ToolInfo(JenkinsTools.CMAKE313, cfg),
		new ToolInfo(JenkinsTools.NINJA, cfg),
		new ToolInfo(JenkinsTools.WIX, cfg),
		new ToolInfo(compiler, cfg)
	]

	// Setup environment according to above toolchain definition.
	withTool(toolchain) {
		dir('serlio.git') {
			String cmd = JenkinsTools.generateSetupPreamble(this, cfg, toolchain.collect { it.tool })
			cmd += "\n"
			cmd += "powershell .\\build.ps1"
			cmd += " -MAYA_DIR ${cfg.maya.p.call(cfg)}" // <-- !!!
			cmd += " -BUILD_NO ${env.BUILD_NUMBER}"
			cmd += " -TARGET InstallerFromScratch"

			psl.runCmd(cmd)

			def buildProps = papl.jpe.readProperties(file: 'build/build_msi/deployment.properties')
			final String artifactPattern = "out/${buildProps.package_file}.*"
			final def artifactVersion = { p -> buildProps.package_version_base }

			// TODO: abusing grp again to label artifact
			def classifierExtractor = { p ->
				return cepl.getArchiveClassifier(cfg) + '-' + cfg.grp
			}
			papl.publish(appName, env.BRANCH_NAME, artifactPattern, artifactVersion, cfg, classifierExtractor)
		}
	}
}